# Baja latencia, en pequeño

Siete programas hosted, sobre atómicos, `align(N)` e hilos (0.32.0 en adelante).

| Caso | Programa | Qué se ve |
|---|---|---|
| Pool de bloques fijos | [pool.lap](pool.lap) | struct de ancho real, punteros, `as` |
| Libro de órdenes | [book.lap](book.lap) | niveles y órdenes por lado, `as` al estrechar |
| Motor | [match.lap](match.lap) | límite y mercado, precio-tiempo, cruce parcial |
| Anillo SPSC | [ring.lap](ring.lap) | `align(64)`, acquire/release, dos hebras |
| False sharing | [share.lap](share.lap) | dos contadores, con y sin `align(64)` en el campo |
| Afinidad | [pin.lap](pin.lap) | `sched_setaffinity` de la hebra actual |
| Watchdog | [watch.lap](watch.lap) | un hilo deja de latir y el otro lo ve |

Cada uno tiene su nota en el directorio del mismo nombre. La salida queda fijada en `tests/expected/examples/lowlat/`.

El pool lleno, una cancelación que no existe y un cruce parcial están en esas salidas. El anillo, antes de crear la segunda hebra, muestra vacío y lleno. Después el productor manda 1..24 y main comprueba el orden y la suma 300. Las 8 ranuras caben: lleno es la diferencia de los índices, no una ranura vacía.

`bench/run.sh` mide el pool, el libro y el motor contra el mismo algoritmo en C y en Rust, con `taskset -c 0`. También mide false sharing (`share` y `split`), afinidad (`pin`, `same` y `free`) y el watchdog, contra C++ y Rust, sin `taskset`. Escribe [RESULTS.md](RESULTS.md). No mide el anillo. La cola de un millón de mensajes, contra C++ y Rust, es [bench/lowlat](../../bench/lowlat/RESULTS.md) (0.32.0). En esa ventana los rangos se solapan. Aquí no se duplica.

Las medianas de este directorio son las de [RESULTS.md](RESULTS.md). El script reescribe el archivo entero.
