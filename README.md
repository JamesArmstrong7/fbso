# FBSO — Fast Boost Structured Orchestrator

**Hardware-Aware Flow Orchestration**  
Versión 0.1 · Licencia BSD

Un núcleo de memoria y estructuras de datos de bajo nivel diseñado para maximizar la localidad espacial, eliminar fricción innecesaria y mantenerse completamente portable.

---

## Estructura del paquete

```
fbso/
├── include/
│   ├── fbso_frame.h        CORE-01  Geometría del Frame (64 bytes)
│   ├── fbso_mutation.h     CORE-02  Bump allocator + Tombstone
│   ├── fbso_compact.h      CORE-03  Compactación (scratchpad externo)
│   ├── fbso_mhal.h         CORE-04  Doble búfer + swap atómico
│   ├── fbso_structures.h   CORE-05  Árboles, grafos y key-value
│   └── fbso_region.h       CORE-07  Regiones + ring de índices (IPC)
│
├── docs/
│   ├── FBSO-CORE-06.md     Capa de I/O agnóstica / Zero-Copy
│   └── FBSO-CORE-07.md     Integración portable Kernel/Userland
│
└── README.md
```

---

## Características

- **C11 puro** (`stdatomic`, `alignas`, `_Static_assert`)
- **Portable**: macOS, Linux, FreeBSD, Apple Silicon, x86-64
- **Modular**: se puede adoptar solo el Frame o el stack completo
- **Zero-allocation** en el camino de datos (el llamador controla la memoria auxiliar)
- **Lock-free** en la ingestión
- **Sin dependencias** externas

---

## Uso mínimo

```c
#include "fbso_mhal.h"
#include "fbso_structures.h"

#define CAP 4096

void *buf_a = aligned_alloc(64, CAP * 64);
void *buf_b = aligned_alloc(64, CAP * 64);

fbso_engine_t engine;
fbso_engine_init(&engine, buf_a, CAP, buf_b, CAP);

/* Escribir */
size_t idx = fbso_engine_append(&engine, payload, len);

/* Navegar un árbol */
fbso_frame_t *node = fbso_frame_at(fbso_engine_active(&engine)->buffer, idx);
fbso_frame_t *left = fbso_tree_left(fbso_engine_active(&engine), node);
```

---

## Documentos canónicos

| Código   | Título                                      |
|----------|---------------------------------------------|
| CORE-01  | Contrato de Memoria y Geometría del Frame   |
| CORE-02  | Primitivas de Mutación (Bump + Tombstone)   |
| CORE-03  | Motor de Compactación Zero-Allocation       |
| CORE-04  | Doble Búfer y Conmutación Atómica           |
| CORE-05  | Estructuras Dinámicas (Árboles / Grafos)    |
| CORE-06  | Capa de I/O Agnóstica                       |
| CORE-07  | Integración Portable Kernel / Userland      |

---

**FBSO v0.1** — Diseño limpio, geometría consciente del hardware, listo para usarse.