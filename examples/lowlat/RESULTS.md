# Baja latencia: pool, libro, motor, false sharing, afinidad y watchdog

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T04:45:25Z — 2026-10-26T04:45:37Z.
Una pasada de calentamiento y siete corridas. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.35.3` `-O2`. Pool, libro y motor: gcc `-O2 -Wall -Werror`, rustc `-O`, `taskset -c 0`.
False sharing, afinidad y watchdog: g++ `-O2 -std=c++17 -Wall -Werror -pthread`, rustc `-O`, sin `taskset`. Esta máquina reporta 4 CPU.
El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en `bench/lowlat` (0.32.0).

## Pool, libro y motor

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 13.0 (12.7–13.3) | 8.9 (8.5–9.2) | 9.0 (8.7–9.1) | por encima | por encima |
| `book` | 2160927 | 49.4 (49.0–49.6) | 38.5 (38.4–38.8) | 33.4 (33.0–33.6) | por encima | por encima |
| `match` | 2853449 | 47.0 (46.7–47.4) | 32.9 (32.6–34.8) | 33.5 (33.1–33.6) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.

## False sharing, afinidad y watchdog

`share` incrementa dos contadores de la misma línea de 64 bytes. `split` los separa con `align(64)` (el segundo queda en el offset 64). `pin` fija las hebras a cpu 0 y cpu 1. `same` fija las dos a cpu 0. `free` no llama a `sched_setaffinity`. Esas tres usan el layout de `split`, así que el tiempo no mezcla el false sharing. `watch` escribe 4000000 latidos y después exige 1000000 lecturas iguales. Si la afinidad falla, `pin` y `same` imprimen 0.

| Programa | Salida | Copihue | C++ | Rust | frente a C++ | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `share` | 8000000 | 149.3 (141.4–166.8) | 143.5 (132.8–155.0) | 147.3 (125.6–164.3) | se solapan | se solapan |
| `split` | 8000000 | 20.2 (20.1–20.5) | 21.0 (20.7–21.2) | 20.7 (20.4–21.2) | por debajo | se solapan |
| `pin` | 8000000 | 20.4 (20.2–29.9) | 20.9 (20.7–21.3) | 20.7 (20.4–21.0) | se solapan | se solapan |
| `same` | 8000000 | 38.5 (38.4–38.6) | 39.4 (39.3–39.5) | 39.2 (39.1–39.3) | por debajo | por debajo |
| `free` | 8000000 | 20.2 (19.9–20.6) | 21.2 (20.9–21.4) | 20.9 (20.3–21.7) | por debajo | se solapan |
| `watch` | 4000000 0 | 2.9 (2.5–3.4) | 3.2 (3.1–3.5) | 2.4 (2.2–2.8) | se solapan | se solapan |

Estas seis filas no van bajo `taskset -c 0`: ese pin deja las dos hebras en un solo núcleo y apaga el false sharing entre núcleos. El efecto de abajo compara los rangos de Copihue entre sí, y el mismo par en C++ y en Rust. `por encima` quiere decir que el mínimo del primero supera el máximo del segundo.

`share` frente a `split`: Copihue por encima, C++ por encima, Rust por encima.
`same` frente a `pin`: Copihue por encima, C++ por encima, Rust por encima.
`pin` frente a `free`: Copihue se solapan, C++ se solapan, Rust se solapan.
`same` frente a `free`: Copihue por encima, C++ por encima, Rust por encima.
