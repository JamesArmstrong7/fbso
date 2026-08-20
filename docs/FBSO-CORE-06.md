# FBSO-CORE-06: Capa de I/O Agnóstica — Zero-Copy Friendly

**Ecosistema:** FBSO Architecture — Hardware-Aware Core  
**Autor:** Dr. Juan — Ph.D. en Ciencias de la Computación, Ingeniería de Software y Mecánica Energética  
**Licencia:** BSD  
**Estado:** Especificación canónica (Fase 2.2)  
**Depende de:** Núcleo v0.1 (CORE-01 … CORE-04) + CORE-05

---

## 1. Abstract y Fundamentación

Una de las propiedades más potentes del diseño FBSO es que los Frames contienen **únicamente offsets relativos y datos empaquetados**. No hay punteros absolutos de memoria virtual.

Esto implica que un buffer de Frames es, en esencia, un **blob de bytes autocontenido**. Puede:

- Escribirse a disco con `write` / `pwrite` / `mmap` + `msync`
- Enviarse por red con `send` / `writev` / `sendmsg`
- Recibirse y mapearse directamente sin deserialización
- Compartirse entre procesos vía memoria compartida

La capa de I/O de FBSO no introduce un nuevo runtime. Solo define **patrones y contratos** para que el tránsito de datos sea lo más cercano posible a zero-copy en macOS, Linux y sistemas similares.

---

## 2. Principios de diseño

1. **El Frame es el formato de intercambio**  
   No se serializa a JSON, protobuf ni formato intermedio. El layout de 64 bytes *es* el formato.

2. **Agnosticismo de medio**  
   El mismo vector de bytes puede ir a NVMe, a una NIC o a otro proceso.

3. **Responsabilidad del llamador**  
   FBSO no oculta las llamadas al sistema. Proporciona helpers y convenciones claras para que el usuario use `mmap`, `writev`, `sendfile`, kqueue, io_uring, etc. según la plataforma.

4. **Compatibilidad con doble búfer**  
   El patrón natural es enviar o persistir el buffer **Frozen** (estable) mientras el **Active** sigue recibiendo escrituras.

---

## 3. Patrones canónicos

### 3.1 Persistencia a disco (mmap)

```c
/* Ejemplo conceptual — macOS / Linux */
int fd = open("frames.dat", O_RDWR | O_CREAT, 0644);
ftruncate(fd, capacity * FBSO_FRAME_SIZE);

void *mapped = mmap(NULL, capacity * FBSO_FRAME_SIZE,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

/* Usar mapped como buffer de un fbso_slice_t */
fbso_slice_t slice;
fbso_slice_init(&slice, mapped, capacity);

/* ... escribir frames ... */

/* Hacer durable */
msync(mapped, capacity * FBSO_FRAME_SIZE, MS_SYNC);
```

Ventaja: el sistema operativo se encarga del write-back. El programa sigue trabajando sobre memoria normal.

### 3.2 Envío por red (vector I/O)

Como los Frames son contiguos, se puede enviar un rango completo con una sola llamada:

```c
/* Enviar desde el índice `start` hasta `head` */
size_t bytes = (head - start) * FBSO_FRAME_SIZE;
const void *ptr = (const uint8_t *)slice.buffer + start * FBSO_FRAME_SIZE;

/* Linux / macOS */
write(fd, ptr, bytes);
/* o send(sock, ptr, bytes, 0); */
```

Para mayor eficiencia se puede usar `writev` / `sendmsg` si se necesitan cabeceras adicionales.

### 3.3 Recepción y rehidratación

El receptor recibe el blob de bytes y lo trata directamente como un array de `fbso_frame_t`, siempre que:

- El buffer de recepción esté alineado a 64 bytes (o se copie a uno alineado).
- Se reconstruya un `fbso_slice_t` con el `head` correcto (puede viajar en una cabecera de mensaje).

No hace falta parsear nodo a nodo.

### 3.4 Integración con el doble búfer

Patrón recomendado:

1. El productor escribe en `Active`.
2. Cuando se decide publicar, se hace `fbso_engine_swap`.
3. El `Frozen` se envía a red o se persiste (es estable).
4. Mientras tanto el nuevo `Active` sigue aceptando datos.
5. Después de la transmisión/persistencia se puede compactar el Frozen o reutilizarlo.

---

## 4. Consideraciones por plataforma

| Plataforma     | Mecanismo recomendado                  | Notas                                      |
|----------------|----------------------------------------|--------------------------------------------|
| macOS          | `mmap` + `msync`, `kqueue`, `sendfile` | Excelente soporte de memoria mapeada       |
| Linux          | `mmap`, `io_uring`, `sendfile`, `writev` | io_uring permite polling y zero-copy avanzado |
| Ambos          | Memoria compartida (`shm_open`)        | Ideal para procesos en la misma máquina    |

FBSO no fuerza el uso de io_uring ni de ninguna API específica. Los patrones funcionan con las llamadas clásicas y se benefician de las modernas cuando están disponibles.

---

## 5. Límites y buenas prácticas

- **Alineación del buffer de recepción**: si el kernel entrega datos no alineados a 64 bytes, hay que copiar a un buffer alineado antes de interpretarlo como Frames. En la práctica, la mayoría de rutas de alto rendimiento permiten controlar la alineación.
- **Cabecera de mensaje**: se recomienda enviar junto con el blob un pequeño header (número de frames, generación, checksum opcional) para que el receptor pueda reconstruir el `fbso_slice_t` correctamente.
- **Endianness**: los offsets y flags son enteros nativos. Si se transmite entre arquitecturas de diferente endianness hay que definir una convención (o restringir a little-endian, el caso dominante).
- **Seguridad**: un buffer de Frames recibido de red debe tratarse como datos no confiables. Validar `head`, rangos de offsets y flags antes de navegar.

---

## 6. Resumen

La capa de I/O de FBSO no es un nuevo framework. Es el reconocimiento de que, una vez que los datos viven en Frames contiguos con offsets relativos, el tránsito hacia disco, red u otros procesos se vuelve conceptualmente trivial y muy eficiente.

El núcleo + estructuras (CORE-01 … CORE-05) ya permiten construir sistemas de alta densidad. CORE-06 solo hace explícito cómo mover esos sistemas a través de las fronteras de I/O sin reintroducir fricción.

---

**Fin de FBSO-CORE-06**