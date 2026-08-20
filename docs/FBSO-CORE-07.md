# FBSO-CORE-07: Integración Portable Kernel / Userland

**Ecosistema:** FBSO Architecture — Hardware-Aware Core  
**Licencia:** BSD  
**Depende de:** CORE-01 … CORE-06

---

## Filosofía

FBSO no impone un kernel ni una API de I/O concreta.  
Toda integración se reduce a:

1. Regiones de memoria contiguas y alineadas a 64 bytes.
2. Paso de índices (no de datos) entre productores y consumidores.
3. Sincronización con atomics C11 en el camino crítico.

El llamador obtiene la memoria como prefiera (`posix_memalign`, `mmap`, `shm_open`, SRAM estática, etc.). FBSO solo valida y opera sobre ella.

---

## Componentes

### `fbso_region_t` + `fbso_region_bind`

Contrato mínimo para describir un buffer de Frames.  
No realiza syscalls. Solo comprueba alineación y tamaño.

### `fbso_index_ring_t`

Cola circular de índices de frame.  
Permite IPC zero-copy entre hilos, procesos o dominios de protección:

- El productor escribe el Frame y publica solo el índice.
- El consumidor lee el índice y accede directamente al Frame en el buffer compartido.

---

## Principios de portabilidad

- Cero llamadas al sistema dentro del núcleo.
- Toda sincronización del fast-path usa `stdatomic.h`.
- El mismo código fuente funciona en macOS, Linux, FreeBSD y entornos bare-metal con cambios mínimos de “glue”.

---

**Fin de FBSO-CORE-07**