#ifndef FBSO_MHAL_H
#define FBSO_MHAL_H

/*
 * FBSO-CORE-04: Doble Búfer y Conmutación Atómica
 * Memory-Hardware Abstraction Layer (mínimo)
 * Licencia: BSD
 *
 * Active  → escritura continua (productor)
 * Frozen  → compactación / lectura estable
 */

#include "fbso_compact.h"
#include <stdatomic.h>

typedef struct {
    fbso_slice_t             slice_a;
    fbso_slice_t             slice_b;
    _Atomic(fbso_slice_t *)  active;
    _Atomic(fbso_slice_t *)  frozen;
    atomic_uint_least64_t    generation;
} fbso_engine_t;

/* ---------- Ciclo de vida ---------- */

static inline int
fbso_engine_init(fbso_engine_t *e,
                 void *buf_a, size_t cap_a,
                 void *buf_b, size_t cap_b)
{
    if (!e || !buf_a || !buf_b || cap_a == 0 || cap_b == 0)
        return -1;

    if (fbso_slice_init(&e->slice_a, buf_a, cap_a) != 0)
        return -1;
    if (fbso_slice_init(&e->slice_b, buf_b, cap_b) != 0) {
        fbso_slice_destroy(&e->slice_a);
        return -1;
    }

    atomic_store_explicit(&e->active, &e->slice_a, memory_order_relaxed);
    atomic_store_explicit(&e->frozen, &e->slice_b, memory_order_relaxed);
    atomic_store_explicit(&e->generation, 0, memory_order_relaxed);

    return 0;
}

static inline void
fbso_engine_destroy(fbso_engine_t *e)
{
    if (!e) return;
    fbso_slice_destroy(&e->slice_a);
    fbso_slice_destroy(&e->slice_b);
}

/* ---------- Camino crítico del productor ---------- */

static inline fbso_slice_t *
fbso_engine_active(fbso_engine_t *e)
{
    return atomic_load_explicit(&e->active, memory_order_acquire);
}

static inline size_t
fbso_engine_append(fbso_engine_t *e, const void *payload, size_t len)
{
    fbso_slice_t *active = fbso_engine_active(e);
    size_t idx = fbso_bump_alloc(active);
    if (idx == (size_t)-1)
        return (size_t)-1;

    fbso_frame_write(active, idx, payload, len);
    return idx;
}

/* ---------- Conmutación atómica ---------- */

static inline void
fbso_engine_swap(fbso_engine_t *e)
{
    fbso_slice_t *old_active = atomic_load_explicit(&e->active, memory_order_relaxed);
    fbso_slice_t *old_frozen = atomic_load_explicit(&e->frozen, memory_order_relaxed);

    atomic_store_explicit(&e->active, old_frozen, memory_order_release);
    atomic_store_explicit(&e->frozen, old_active, memory_order_release);

    atomic_fetch_add_explicit(&e->generation, 1, memory_order_relaxed);
}

/* ---------- Compactación del Frozen ---------- */

/**
 * Compacta el buffer Frozen actual hacia un destino temporal.
 * El llamador suministra tanto el slice de destino como el scratchpad de remap.
 *
 * Patrón típico:
 *   1. fbso_engine_compact_frozen(...)
 *   2. (opcional) intercambiar o copiar el resultado
 *   3. reiniciar el Frozen para el próximo ciclo
 */
static inline size_t
fbso_engine_compact_frozen(fbso_engine_t *e,
                           fbso_slice_t *temp_dst,
                           size_t *remap_scratch)
{
    fbso_slice_t *frozen = atomic_load_explicit(&e->frozen, memory_order_acquire);
    if (!frozen || !temp_dst || !remap_scratch)
        return (size_t)-1;

    return fbso_compact(frozen, temp_dst, remap_scratch);
}

#endif /* FBSO_MHAL_H */
