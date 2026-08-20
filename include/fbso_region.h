#ifndef FBSO_REGION_H
#define FBSO_REGION_H

/*
 * FBSO-CORE-07: Integración Portable — Regiones y Ring de Índices
 * Licencia: BSD
 *
 * Contratos mínimos y agnósticos de plataforma.
 * El llamador es responsable de obtener la memoria alineada.
 */

#include "fbso_frame.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 *  Región de memoria
 * ============================================================ */

typedef enum {
    FBSO_MEM_ANONYMOUS = 0,   /* RAM normal alineada */
    FBSO_MEM_SHARED    = 1,   /* memoria compartida entre procesos */
    FBSO_MEM_FILE      = 2,   /* respaldada por archivo */
    FBSO_MEM_CUSTOM    = 3    /* el usuario aporta el puntero ya listo */
} fbso_mem_kind_t;

typedef struct {
    void            *base;    /* debe estar alineado a 64 bytes */
    size_t           size;    /* múltiplo de FBSO_FRAME_SIZE */
    fbso_mem_kind_t  kind;
    int              fd;      /* opcional, -1 si no aplica */
} fbso_region_t;

/**
 * Valida y vincula una región ya asignada por el llamador.
 * No realiza ninguna llamada al sistema.
 */
static inline int
fbso_region_bind(fbso_region_t *reg, void *aligned_base, size_t size,
                 fbso_mem_kind_t kind, int fd)
{
    if (!reg || !aligned_base || size < FBSO_FRAME_SIZE)
        return -1;
    if ((uintptr_t)aligned_base % FBSO_CACHELINE_SIZE != 0)
        return -1;
    if (size % FBSO_FRAME_SIZE != 0)
        return -1;

    reg->base = aligned_base;
    reg->size = size;
    reg->kind = kind;
    reg->fd   = fd;
    return 0;
}

/* ============================================================
 *  Ring de índices (IPC / zero-copy entre productores y consumidores)
 * ============================================================ */

typedef struct {
    alignas(FBSO_CACHELINE_SIZE) atomic_size_t head;
    alignas(FBSO_CACHELINE_SIZE) atomic_size_t tail;
    size_t capacity;          /* se recomienda potencia de 2 */
    uint32_t indices[];       /* flexible array member */
} fbso_index_ring_t;

/**
 * Calcula el tamaño total necesario para un ring de la capacidad dada.
 */
static inline size_t
fbso_index_ring_bytes(size_t capacity)
{
    return sizeof(fbso_index_ring_t) + capacity * sizeof(uint32_t);
}

/**
 * Inicializa un ring sobre memoria ya reservada y alineada.
 */
static inline int
fbso_index_ring_init(fbso_index_ring_t *ring, size_t capacity)
{
    if (!ring || capacity == 0)
        return -1;

    atomic_store_explicit(&ring->head, 0, memory_order_relaxed);
    atomic_store_explicit(&ring->tail, 0, memory_order_relaxed);
    ring->capacity = capacity;
    return 0;
}

/**
 * Publica un índice. Devuelve 0 en éxito, -1 si el ring está lleno.
 * Un solo productor recomendado (o sincronización externa).
 */
static inline int
fbso_index_ring_push(fbso_index_ring_t *ring, uint32_t index)
{
    size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    size_t next = (head + 1) % ring->capacity;
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

    if (next == tail)
        return -1; /* lleno */

    ring->indices[head] = index;
    atomic_store_explicit(&ring->head, next, memory_order_release);
    return 0;
}

/**
 * Consume un índice. Devuelve 0 en éxito y escribe el índice,
 * -1 si el ring está vacío.
 */
static inline int
fbso_index_ring_pop(fbso_index_ring_t *ring, uint32_t *out_index)
{
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);

    if (tail == head)
        return -1; /* vacío */

    *out_index = ring->indices[tail];
    atomic_store_explicit(&ring->tail, (tail + 1) % ring->capacity, memory_order_release);
    return 0;
}

#endif /* FBSO_REGION_H */
