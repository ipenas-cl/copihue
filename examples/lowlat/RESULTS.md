# Baja latencia: pool, libro y motor

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T00:51:15Z — 2026-10-26T00:51:19Z.
Una pasada de calentamiento y siete corridas, `taskset -c 0`. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.32.1` `-O2`. gcc `-O2 -Wall -Werror`. rustc `-O`.
El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en `bench/lowlat` (0.32.0). Este archivo mide el pool de punteros, el libro y el motor.

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 20.8 (20.6–20.9) | 8.9 (8.6–9.2) | 8.9 (8.7–9.2) | por encima | por encima |
| `book` | 2160927 | 49.3 (49.2–49.8) | 38.7 (38.4–38.9) | 33.3 (33.0–33.5) | por encima | por encima |
| `match` | 2853449 | 47.1 (46.8–47.4) | 32.9 (32.7–33.4) | 33.6 (33.4–34.3) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.
