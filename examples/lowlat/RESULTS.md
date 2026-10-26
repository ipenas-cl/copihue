# Baja latencia: pool, libro, motor, false sharing, afinidad y watchdog

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T04:51:05Z — 2026-10-26T04:51:17Z.
Una pasada de calentamiento y siete corridas. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.36.1` `-O2`. Pool, libro y motor: gcc `-O2 -Wall -Werror`, rustc `-O`, `taskset -c 0`.
False sharing, afinidad y watchdog: g++ `-O2 -std=c++17 -Wall -Werror -pthread`, rustc `-O`, sin `taskset`. Esta máquina reporta 4 CPU.
El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en `bench/lowlat` (0.32.0).

## Pool, libro y motor

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 13.1 (12.9–13.4) | 8.7 (8.6–9.0) | 8.9 (8.4–9.2) | por encima | por encima |
| `book` | 2160927 | 49.5 (49.4–50.0) | 38.5 (38.1–38.8) | 33.3 (32.7–33.5) | por encima | por encima |
| `match` | 2853449 | 47.0 (46.9–47.4) | 33.0 (32.7–33.2) | 33.5 (33.0–33.7) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.

## False sharing, afinidad y watchdog

`share` incrementa dos contadores de la misma línea de 64 bytes. `split` los separa con `align(64)` (el segundo queda en el offset 64). `pin` fija las hebras a cpu 0 y cpu 1. `same` fija las dos a cpu 0. `free` no llama a `sched_setaffinity`. Esas tres usan el layout de `split`, así que el tiempo no mezcla el false sharing. `watch` escribe 4000000 latidos y después exige 1000000 lecturas iguales. Si la afinidad falla, `pin` y `same` imprimen 0.

| Programa | Salida | Copihue | C++ | Rust | frente a C++ | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `share` | 8000000 | 146.8 (135.6–166.7) | 150.1 (143.2–155.9) | 152.6 (141.1–157.0) | se solapan | se solapan |
| `split` | 8000000 | 20.3 (19.9–20.6) | 21.3 (20.7–23.1) | 20.9 (20.6–21.1) | por debajo | se solapan |
| `pin` | 8000000 | 20.4 (19.9–28.2) | 21.2 (20.9–21.6) | 21.0 (20.6–21.1) | se solapan | se solapan |
| `same` | 8000000 | 38.5 (38.4–40.3) | 39.5 (39.4–39.7) | 39.2 (39.0–39.6) | se solapan | se solapan |
| `free` | 8000000 | 20.0 (19.9–20.2) | 21.0 (20.6–21.5) | 20.8 (20.4–23.2) | por debajo | por debajo |
| `watch` | 4000000 0 | 2.7 (2.4–3.7) | 3.3 (3.0–3.4) | 2.7 (2.3–3.2) | se solapan | se solapan |

Estas seis filas no van bajo `taskset -c 0`: ese pin deja las dos hebras en un solo núcleo y apaga el false sharing entre núcleos. El efecto de abajo compara los rangos de Copihue entre sí, y el mismo par en C++ y en Rust. `por encima` quiere decir que el mínimo del primero supera el máximo del segundo.

`share` frente a `split`: Copihue por encima, C++ por encima, Rust por encima.
`same` frente a `pin`: Copihue por encima, C++ por encima, Rust por encima.
`pin` frente a `free`: Copihue se solapan, C++ se solapan, Rust se solapan.
`same` frente a `free`: Copihue por encima, C++ por encima, Rust por encima.
