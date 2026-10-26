# Baja latencia, en pequeño

Cuatro programas hosted. No hay hilos. Los atómicos no están: llegan en 0.32.0.

| Caso | Programa | Qué se ve |
|---|---|---|
| Pool de bloques fijos | [pool.lap](pool.lap) | struct de ancho real, punteros, `as` |
| Libro de órdenes | [book.lap](book.lap) | niveles y órdenes por lado, `as` al estrechar |
| Motor | [match.lap](match.lap) | límite y mercado, precio-tiempo, cruce parcial |
| Anillo | [ring.lap](ring.lap) | índices `volatile`, máscara potencia de 2 |

Cada uno tiene su nota en el directorio del mismo nombre. La salida queda fijada en `tests/expected/examples/lowlat/`.

El pool lleno, una cancelación que no existe y un cruce parcial están en esas salidas. El anillo también muestra lleno y vacío. Una ranura queda libre para distinguir los dos.

`bench/run.sh` mide el pool, el libro y el motor contra el mismo algoritmo en C y en Rust. Escribe [RESULTS.md](RESULTS.md). El anillo no entra: sin atómicos no hay una cola SPSC de dos hebras que medir.

En la corrida que dejó ese archivo (2026-10-26T00:27:12Z — 2026-10-26T00:27:15Z, binario 0.31.2) Copihue queda por encima de C y de Rust en las tres filas. Los rangos no se solapan. Pool 20.8 ms (20.2–23.4) contra C 9.1 (8.5–9.3) y Rust 9.0 (8.6–9.3). Libro 49.6 (49.3–50.0) contra C 38.7 (38.3–39.1) y Rust 33.4 (33.2–33.5). Motor 47.1 (46.8–47.3) contra C 33.2 (32.9–33.4) y Rust 33.6 (33.1–34.4). No hay una fila ganada. El lenguaje es el de 0.31.0; 0.31.1, ya en main, no cambia estos programas.
