# Baja latencia: pool, libro y motor

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T04:05:14Z — 2026-10-26T04:05:17Z.
Una pasada de calentamiento y siete corridas, `taskset -c 0`. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.35.1` `-O2`. gcc `-O2 -Wall -Werror`. rustc `-O`.
El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en `bench/lowlat` (0.32.0). Este archivo mide el pool de punteros, el libro y el motor.

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 13.0 (12.6–13.2) | 8.8 (8.5–9.1) | 9.1 (8.7–9.5) | por encima | por encima |
| `book` | 2160927 | 49.4 (49.1–49.9) | 38.6 (38.3–38.9) | 33.4 (32.9–33.5) | por encima | por encima |
| `match` | 2853449 | 47.1 (46.9–47.5) | 33.0 (32.8–33.3) | 33.2 (32.8–33.5) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.
