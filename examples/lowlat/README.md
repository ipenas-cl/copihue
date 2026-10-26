# Baja latencia, en pequeño

Cuatro programas hosted, sobre 0.32.0 (atómicos, `align(N)` e hilos).

| Caso | Programa | Qué se ve |
|---|---|---|
| Pool de bloques fijos | [pool.lap](pool.lap) | struct de ancho real, punteros, `as` |
| Libro de órdenes | [book.lap](book.lap) | niveles y órdenes por lado, `as` al estrechar |
| Motor | [match.lap](match.lap) | límite y mercado, precio-tiempo, cruce parcial |
| Anillo SPSC | [ring.lap](ring.lap) | `align(64)`, acquire/release, dos hebras |

Cada uno tiene su nota en el directorio del mismo nombre. La salida queda fijada en `tests/expected/examples/lowlat/`.

El pool lleno, una cancelación que no existe y un cruce parcial están en esas salidas. El anillo, antes de crear la segunda hebra, muestra vacío y lleno. Después el productor manda 1..24 y main comprueba el orden y la suma 300. Las 8 ranuras caben: lleno es la diferencia de los índices, no una ranura vacía.

`bench/run.sh` mide el pool, el libro y el motor contra el mismo algoritmo en C y en Rust. Escribe [RESULTS.md](RESULTS.md). No mide el anillo. La cola de un millón de mensajes, contra C++ y Rust, es [bench/lowlat](../../bench/lowlat/RESULTS.md) (0.32.0). En esa ventana los rangos se solapan. Aquí no se duplica.

En la corrida que dejó ese archivo (2026-10-26T00:51:15Z — 2026-10-26T00:51:19Z, binario 0.32.1) Copihue queda por encima de C y de Rust en las tres filas. Los rangos no se solapan. Pool 20.8 ms (20.6–20.9) contra C 8.9 (8.6–9.2) y Rust 8.9 (8.7–9.2). Libro 49.3 (49.2–49.8) contra C 38.7 (38.4–38.9) y Rust 33.3 (33.0–33.5). Motor 47.1 (46.8–47.4) contra C 32.9 (32.7–33.4) y Rust 33.6 (33.4–34.3). No hay una fila ganada. Frente a la ventana anterior de estos ejemplos (00:27:12Z — 00:27:15Z, binario 0.31.2) cada rango de Copihue se solapa con el anterior: esta versión no mueve el pool, el libro ni el motor. El código generado es el de 0.32.0.
