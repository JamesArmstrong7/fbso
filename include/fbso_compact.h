#ifndef FBSO_COMPACT_H
#define FBSO_COMPACT_H

/*
 * FBSO-CORE-03: Motor de Compactación Zero-Allocation
 * Licencia: BSD
 *
 * Corrección: el mapa de reindexación es suministrado por el llamador
 * (scratchpad). No hay calloc interno en el camino de datos.
 */

#include "fbso_mutation.h"

/**
 * Compacta src → dst.
 *
 * @param src           Slice de origen (puede contener tombstones)
 * @param dst           Slice de destino (debe estar vacío y con capacidad suficiente)
 * @param remap         Scratchpad de size_t[src->capacity] suministrado por el llamador
 * @return              Número de frames vivos copiados, o (size_t)-1 en error
 */
static inline size_t
fbso_compact(const fbso_slice_t *src, fbso_slice_t *dst, size_t *remap)
{
    if (!src || !dst || !src->buffer || !dst->buffer || !remap)
        return (size_t)-1;

    size_t old_head = atomic_load_explicit(&src->head, memory_order_acquire);

    for (size_t i = 0; i < old_head; ++i)
        remap[i] = (size_t)-1;

    size_t new_idx = 0;

    /* Pasada 1: copiar frames vivos y construir mapa old → new */
    for (size_t old = 0; old < old_head; ++old) {
        if (!fbso_is_alive(src, old))
            continue;

        if (new_idx >= dst->capacity)
            return (size_t)-1;

        fbso_frame_t *src_f = fbso_frame_at(src->buffer, old);
        fbso_frame_t *dst_f = fbso_frame_at(dst->buffer, new_idx);

        memcpy(dst_f, src_f, FBSO_FRAME_SIZE);
        remap[old] = new_idx;
        new_idx++;
    }

    /* Pasada 2: reindexar offsets relativos */
    for (size_t i = 0; i < new_idx; ++i) {
        fbso_frame_t *f = fbso_frame_at(dst->buffer, i);

        if (f->relative_left != FBSO_OFFSET_NULL) {
            size_t old_left = f->relative_left;
            f->relative_left = (old_left < old_head && remap[old_left] != (size_t)-1)
                               ? (fbso_offset16_t)remap[old_left]
                               : FBSO_OFFSET_NULL;
        }

        if (f->relative_right != FBSO_OFFSET_NULL) {
            size_t old_right = f->relative_right;
            f->relative_right = (old_right < old_head && remap[old_right] != (size_t)-1)
                                ? (fbso_offset16_t)remap[old_right]
                                : FBSO_OFFSET_NULL;
        }
    }

    atomic_store_explicit(&dst->head, new_idx, memory_order_release);

    /* Todo el destino queda vivo */
    for (size_t w = 0; w < dst->tombstone_words; ++w)
        atomic_store_explicit(&dst->tombstone[w], 0, memory_order_relaxed);

    return new_idx;
}

#endif /* FBSO_COMPACT_H */
