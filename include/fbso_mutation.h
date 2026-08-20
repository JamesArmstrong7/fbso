#ifndef FBSO_MUTATION_H
#define FBSO_MUTATION_H

/*
 * FBSO-CORE-02: Primitivas de Mutación
 * Bump Allocator + Invalidación Lógica (Tombstone)
 * Licencia: BSD
 *
 * Correcciones aplicadas:
 *  - Inicialización de atomics con atomic_init (sin UB)
 *  - Barrera de memoria release en la publicación del frame
 */

#include "fbso_frame.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    uint8_t                 *buffer;          /* buffer contiguo alineado */
    size_t                   capacity;        /* número máximo de frames */
    atomic_size_t            head;            /* siguiente índice libre */
    atomic_uint_least64_t   *tombstone;       /* bitmap de invalidación */
    size_t                   tombstone_words; /* palabras de 64 bits */
} fbso_slice_t;

/* ---------- Inicialización / destrucción ---------- */

static inline int
fbso_slice_init(fbso_slice_t *s, void *aligned_buffer, size_t capacity)
{
    if (!s || !aligned_buffer || capacity == 0)
        return -1;

    s->buffer   = (uint8_t *)aligned_buffer;
    s->capacity = capacity;
    atomic_store_explicit(&s->head, 0, memory_order_relaxed);

    s->tombstone_words = (capacity + 63) / 64;
    s->tombstone = malloc(s->tombstone_words * sizeof(atomic_uint_least64_t));
    if (!s->tombstone)
        return -1;

    /* Inicialización conforme a ISO C11 — evita UB */
    for (size_t i = 0; i < s->tombstone_words; ++i)
        atomic_init(&s->tombstone[i], 0);

    return 0;
}

static inline void
fbso_slice_destroy(fbso_slice_t *s)
{
    if (s && s->tombstone) {
        free(s->tombstone);
        s->tombstone = NULL;
    }
}

/* ---------- Bump allocation ---------- */

static inline size_t
fbso_bump_alloc(fbso_slice_t *s)
{
    size_t idx = atomic_fetch_add_explicit(&s->head, 1, memory_order_relaxed);
    if (idx >= s->capacity)
        return (size_t)-1;
    return idx;
}

/* ---------- Escritura con orden de memoria correcto ---------- */

static inline fbso_frame_t *
fbso_frame_write(fbso_slice_t *s, size_t idx, const void *payload, size_t len)
{
    if (idx >= s->capacity || len > 56)
        return NULL;

    fbso_frame_t *f = fbso_frame_at(s->buffer, idx);

    f->relative_left  = FBSO_OFFSET_NULL;
    f->relative_right = FBSO_OFFSET_NULL;
    f->flags          = 0;

    if (payload && len > 0)
        memcpy(f->payload, payload, len);

    /* Todo el payload debe ser visible antes de marcar el frame como vivo */
    atomic_thread_fence(memory_order_release);

    size_t word = idx / 64;
    uint64_t bit  = 1ULL << (idx % 64);
    atomic_fetch_and_explicit(&s->tombstone[word], ~bit, memory_order_relaxed);

    return f;
}

/* ---------- Invalidación lógica ---------- */

static inline void
fbso_invalidate(fbso_slice_t *s, size_t idx)
{
    if (idx >= s->capacity)
        return;

    size_t word = idx / 64;
    uint64_t bit  = 1ULL << (idx % 64);
    atomic_fetch_or_explicit(&s->tombstone[word], bit, memory_order_release);
}

static inline bool
fbso_is_alive(const fbso_slice_t *s, size_t idx)
{
    if (idx >= s->capacity)
        return false;

    size_t word = idx / 64;
    uint64_t bit  = 1ULL << (idx % 64);
    uint64_t mask = atomic_load_explicit(&s->tombstone[word], memory_order_acquire);
    return (mask & bit) == 0;
}

#endif /* FBSO_MUTATION_H */
