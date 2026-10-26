# Baja latencia: pool, libro y motor

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T02:32:11Z — 2026-10-26T02:32:14Z.
Una pasada de calentamiento y siete corridas, `taskset -c 0`. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.33.1` `-O2`. gcc `-O2 -Wall -Werror`. rustc `-O`.
El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en `bench/lowlat` (0.32.0). Este archivo mide el pool de punteros, el libro y el motor.

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 14.2 (13.6–15.1) | 8.8 (8.5–9.1) | 8.9 (8.4–9.3) | por encima | por encima |
| `book` | 2160927 | 49.5 (49.1–51.6) | 38.6 (38.1–38.7) | 33.2 (32.9–33.6) | por encima | por encima |
| `match` | 2853449 | 47.1 (47.0–47.3) | 33.0 (32.5–33.1) | 33.3 (33.1–33.6) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.
