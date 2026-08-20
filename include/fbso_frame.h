#ifndef FBSO_FRAME_H
#define FBSO_FRAME_H

/*
 * FBSO-CORE-01: Contrato de Memoria y Geometría del Frame
 * Licencia: BSD
 *
 * Frame de exactamente 64 bytes alineado a línea de caché L1.
 * Solo se permiten offsets relativos. Prohibidos los punteros absolutos.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>

#define FBSO_CACHELINE_SIZE  64
#define FBSO_FRAME_SIZE      FBSO_CACHELINE_SIZE

/* Offsets de 16 bits → hasta 65535 frames (~4 MiB) */
typedef uint16_t fbso_offset16_t;

/* Variante de 32 bits para buffers mayores */
typedef uint32_t fbso_offset32_t;

/* Valor especial = “sin enlace” */
#define FBSO_OFFSET_NULL  ((fbso_offset16_t)0xFFFF)

/*
 * Layout canónico (64 bytes exactos):
 *   0-1   relative_left
 *   2-3   relative_right
 *   4-7   flags
 *   8-63  payload[56]
 *
 * Nota de alineación del payload:
 * El payload comienza en el offset 8. Si el usuario almacena tipos de
 * 8 bytes (uint64_t, punteros, structs) dentro del payload, es su
 * responsabilidad garantizar la alineación correcta (alignas, packing
 * manual o copias byte a byte). El layout del Frame no introduce
 * padding interno en x86-64 ni ARM64/Apple Silicon.
 */
typedef struct alignas(FBSO_CACHELINE_SIZE) {
    fbso_offset16_t  relative_left;
    fbso_offset16_t  relative_right;
    uint32_t         flags;
    uint8_t          payload[56];
} fbso_frame_t;

_Static_assert(sizeof(fbso_frame_t) == FBSO_FRAME_SIZE,
               "FBSO: el Frame no mide exactamente 64 bytes");
_Static_assert(alignof(fbso_frame_t) == FBSO_CACHELINE_SIZE,
               "FBSO: el Frame no está alineado a 64 bytes");
_Static_assert(offsetof(fbso_frame_t, payload) == 8,
               "FBSO: el payload no comienza en el offset 8");

/* Variante con offsets de 32 bits (payload reducido a 52 bytes) */
typedef struct alignas(FBSO_CACHELINE_SIZE) {
    fbso_offset32_t  relative_left;
    fbso_offset32_t  relative_right;
    uint32_t         flags;
    uint8_t          payload[52];
} fbso_frame32_t;

_Static_assert(sizeof(fbso_frame32_t) == FBSO_FRAME_SIZE,
               "FBSO: frame32 no mide 64 bytes");

/* ---------- Helpers de navegación ---------- */

static inline fbso_frame_t *
fbso_frame_at(void *base, size_t index)
{
    return (fbso_frame_t *)((uint8_t *)base + index * FBSO_FRAME_SIZE);
}

static inline size_t
fbso_offset_to_index(fbso_offset16_t off)
{
    return (off == FBSO_OFFSET_NULL) ? (size_t)-1 : (size_t)off;
}

static inline fbso_offset16_t
fbso_index_to_offset(size_t index)
{
    return (index > 0xFFFE) ? FBSO_OFFSET_NULL : (fbso_offset16_t)index;
}

#endif /* FBSO_FRAME_H */
