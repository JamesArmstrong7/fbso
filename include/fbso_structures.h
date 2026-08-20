#ifndef FBSO_STRUCTURES_H
#define FBSO_STRUCTURES_H

/*
 * FBSO-CORE-05: Abstracción de Estructuras Dinámicas
 * Árboles, Grafos e Índices sobre Frames
 * Licencia: BSD
 *
 * Depende de: fbso_frame.h + fbso_mutation.h (núcleo v0.1)
 */

#include "fbso_frame.h"
#include "fbso_mutation.h"

/* ============================================================
 *  Navegación de Árbol Binario
 * ============================================================ */

static inline fbso_frame_t *
fbso_tree_left(fbso_slice_t *s, const fbso_frame_t *node)
{
    if (!s || !node || node->relative_left == FBSO_OFFSET_NULL)
        return NULL;
    return fbso_frame_at(s->buffer, node->relative_left);
}

static inline fbso_frame_t *
fbso_tree_right(fbso_slice_t *s, const fbso_frame_t *node)
{
    if (!s || !node || node->relative_right == FBSO_OFFSET_NULL)
        return NULL;
    return fbso_frame_at(s->buffer, node->relative_right);
}

static inline void
fbso_tree_set_left(fbso_frame_t *parent, size_t child_idx)
{
    if (parent)
        parent->relative_left = fbso_index_to_offset(child_idx);
}

static inline void
fbso_tree_set_right(fbso_frame_t *parent, size_t child_idx)
{
    if (parent)
        parent->relative_right = fbso_index_to_offset(child_idx);
}

/* ============================================================
 *  Payload canónico Clave-Valor (árboles de búsqueda / índices)
 * ============================================================ */

typedef struct {
    uint64_t key;
    uint8_t  value[48];
} fbso_kv_payload_t;

_Static_assert(sizeof(fbso_kv_payload_t) <= 56,
               "FBSO: fbso_kv_payload_t excede el payload del Frame");

/* ============================================================
 *  Grafo denso de bajo grado (lista de adyacencia embebida)
 *  Máximo 27 vecinos de 16 bits + degree
 * ============================================================ */

typedef struct {
    uint16_t        degree;
    fbso_offset16_t neighbors[27];
} fbso_adj_payload_t;

_Static_assert(sizeof(fbso_adj_payload_t) <= 56,
               "FBSO: fbso_adj_payload_t excede el payload del Frame");

/* Helpers de grafo de bajo grado */
static inline int
fbso_adj_add_neighbor(fbso_adj_payload_t *adj, fbso_offset16_t neighbor)
{
    if (!adj || adj->degree >= 27)
        return -1;
    adj->neighbors[adj->degree++] = neighbor;
    return 0;
}

static inline fbso_frame_t *
fbso_adj_neighbor(fbso_slice_t *s, const fbso_adj_payload_t *adj, uint16_t i)
{
    if (!s || !adj || i >= adj->degree || adj->neighbors[i] == FBSO_OFFSET_NULL)
        return NULL;
    return fbso_frame_at(s->buffer, adj->neighbors[i]);
}

#endif /* FBSO_STRUCTURES_H */
