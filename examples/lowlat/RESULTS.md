# Baja latencia: pool, libro y motor

Generado por `examples/lowlat/bench/run.sh`. Muestras 2026-10-26T00:27:12Z — 2026-10-26T00:27:15Z.
Una pasada de calentamiento y siete corridas, `taskset -c 0`. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas.
Copihue `Copihue 0.31.2` `-O2`. gcc `-O2 -Wall -Werror`. rustc `-O`.
El anillo no se mide: es de una hebra y no hay atómicos.

| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |
|---|---:|---:|---:|---:|---|---|
| `pool` | 2052000 | 20.8 (20.2–23.4) | 9.1 (8.5–9.3) | 9.0 (8.6–9.3) | por encima | por encima |
| `book` | 2160927 | 49.6 (49.3–50.0) | 38.7 (38.3–39.1) | 33.4 (33.2–33.5) | por encima | por encima |
| `match` | 2853449 | 47.1 (46.8–47.3) | 33.2 (32.9–33.4) | 33.6 (33.1–34.4) | por encima | por encima |

`por debajo` quiere decir que el máximo de Copihue es menor que el mínimo del otro. `se solapan` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape.
Rust del pool usa `unsafe` porque la lista libre es de punteros. El libro y el motor son índices, sin `unsafe`.
