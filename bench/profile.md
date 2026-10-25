## Perfil del código generado

El perfil es un recuento estático del ensamblado `-O2` que emitieron los dos compiladores medidos más abajo. No es un perfil de ciclos: `perf` no está instalado en esta máquina. Una instrucción cuenta una vez por aparición. No se contaron comentarios ni líneas en blanco. `mov rbx, rax` es el paso de la máquina de pila (el lado izquierdo ya estaba en `rax`). Una carga es un uso de `[rbp…]` que no es el destino; un store es `mov [rbp…]`, `mov byte [rbp…]` o `mov qword [rbp…]`.

El bloque `Lbounds_error_*` (8 instrucciones, `syscall` de salida) no está en el camino: los índices de estos programas caen dentro del arreglo. El `jmp` al bloque `Lbounds_ok` sí corre, y entra en la cuenta. El `jmp` de vuelta al cabeza del bucle es una etiqueta de una instrucción y también entra.

Ningún bucle de `arith`, `fold`, `chain`, `array`, `matmul`, `sieve` ni `sort` contiene un `call`. El `call` a `print_int` está después del bucle, dentro de `main`. `main` no es una función hoja. Un reparto de registros que solo cubriera hojas no habría movido esas siete filas. `fib` tampoco es hoja: cada invocación con `n >= 2` hace dos `call fib`. No se contaron invocaciones en ejecución.

El bucle que se cuenta es el más interno de cada programa. En `sort` es una vuelta del desplazamiento (`j >= 0 && a[j] > key`, camino en que las dos comparaciones salen verdaderas); cuántas vueltas da depende de los datos, y no se multiplica. En la criba es `while (j < limit) { flags[j] = 0; j = j + i; }`. Las vueltas de las otras filas salen del fuente.

| Programa | Bucle contado | Vueltas en el fuente | 0.22.2 ins (push / pop / `mov rbx,rax` / cargas / stores / `call`) | 0.22.3 |
|---|---|---:|---|---|
| `arith` | `.L0` + `jmp` | 100000000 | 54 (8 / 8 / 8 / 9 / 4 / 0) | 30 (0 / 0 / 0 / 1 / 0 / 0) |
| `fold` | `.L0` + `jmp` | 500000000 | 35 (5 / 5 / 5 / 5 / 2 / 0) | 20 (0 / 0 / 0 / 0 / 0 / 0) |
| `chain` | `.L0` + `jmp` | 80000000 | 55 (8 / 8 / 8 / 8 / 4 / 0) | 30 (0 / 0 / 0 / 0 / 0 / 0) |
| `array` | cuerpo interno, índice dentro de rango | 100000 × 1000 | 60 (8 / 8 / 9 / 8 / 3 / 0) | 39 (1 / 1 / 2 / 2 / 2 / 0) |
| `matmul` | bucle `k`, dos índices dentro de rango | 384³ = 56623104 | 66 (8 / 8 / 10 / 12 / 2 / 0) | 48 (2 / 2 / 4 / 3 / 1 / 0) |
| `sieve` | marcado `flags[j] = 0` | no es una constante del fuente | 24 (3 / 3 / 3 / 5 / 2 / 0) | 18 (1 / 1 / 1 / 0 / 1 / 0) |
| `sort` | una vuelta del desplazamiento | depende de los datos | 60 (5 / 5 / 7 / 8 / 2 / 0) | 48 (1 / 1 / 3 / 2 / 1 / 0) |
| `fib` | cuerpo con `n >= 2`, hasta el primer `ret` | no se midió | 38 (7 / 6 / 4 / 3 / 1 / 2) | 31 (4 / 3 / 1 / 1 / 1 / 2) |

En `arith` 0.22.2, 24 de las 54 instrucciones son `push`, `pop` o `mov rbx, rax`. Cada local se releía de `[rbp+off]`. El 0.22.3 deja `a`, `b`, `c` e `i` en `r15`, `r12`, `r13` y `r14`. `n` es el quinto escalar y sigue en `[rbp-8]`: es la única carga que queda en ese bucle. El valor sigue pasando por `rax`/`rbx`; el registro no es el acumulador.

En `fib` 0.22.3, `n` queda en `r12` y sobrevive a los dos `call` porque el callee guarda `r12`. El resultado del primer `call` sigue haciendo `push`/`pop`: el segundo `call` no cabe en `rbx`.

`text` según `size`, bytes, 0.22.2 → 0.22.3: `fib` 431 → 427, `sieve` 1121 → 993, `matmul` 1514 → 1258, `arith` 533 → 509, `chain` 500 → 470, `fold` 436 → 436, `array` 1051 → 926, `sort` 1246 → 1110.

## Antes y después, mismo `make bench`

Dos corridas distintas, la misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld que la tabla de arriba. Copihue 0.22.2: muestras 2026-10-25 13:43:41 UTC — 13:44:37 UTC. Copihue 0.22.3: muestras 2026-10-25 13:56:57 UTC — 13:57:43 UTC, que son las de la tabla de esta página. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.22.2 / 0.22.3.

| Programa | 0.22.2 | 0.22.3 | 0.22.2 / 0.22.3 |
|---|---:|---:|---:|
| `fib` | 735.0 (726.6–738.0) | 456.7 (456.1–459.3) | 1.61 |
| `sieve` | 181.7 (180.4–206.8) | 147.7 (146.7–148.5) | 1.23 |
| `matmul` | 171.1 (162.3–176.7) | 115.4 (114.5–125.8) | 1.48 |
| `arith` | 235.5 (233.9–236.4) | 130.9 (130.6–132.6) | 1.80 |
| `chain` | 207.2 (203.6–209.6) | 169.1 (168.9–169.8) | 1.23 |
| `fold` | 753.2 (734.3–817.8) | 419.9 (419.8–422.8) | 1.79 |
| `array` | 270.5 (259.9–276.2) | 165.0 (164.3–165.3) | 1.64 |
| `sort` | 494.1 (486.1–542.1) | 377.6 (376.9–377.7) | 1.31 |

Las ocho medianas de Copihue bajaron. Entre las dos ventanas, C, Rust y OCaml se movieron unos pocos milisegundos (en `fib`, C pasó de 130.8 a 131.3). La bajada de Copihue es más grande que ese movimiento en las ocho filas.

`chain` es la única fila en que la mediana de Copihue 0.22.3 queda por debajo de C, de Rust y de OCaml. Los rangos no se solapan: Copihue 168.9–169.8, C 188.0–189.7, Rust 182.8–187.6, OCaml 187.6–191.0. Eso es esta fila y esta corrida. Las otras siete siguen por encima.

`fold` sigue en 419.9 ms. gcc y rustc tienen el decimal del resultado en el `.s` y tardan 2.2 y 1.9 ms; no ejecutaron el bucle. OCaml sí lo ejecuta (234.2 ms). El cociente Copihue/OCaml pasó de 3.50 en la corrida 0.22.2 a 1.79 en esta. Este cambio no borra bucles.

En `array`, el local tardó 165.0 ms (164.3–165.3) y el global 156.4 ms (156.2–157.7). Los rangos no se solapan. La diferencia es 8.6 ms, alrededor del 5 % del local. El chequeo de límites se ve, y no es el costo principal de esa fila.

Lo que estas corridas dejan como siguiente paso, en este orden: usar el registro como acumulador en vez de copiar por `rax`/`rbx`, que es lo que aún se ve en `arith` (2.19× C, 2.32× Rust) junto con el quinto local en memoria; borrar el bucle de `fold`, que es la distancia contra gcc y rustc y que este cambio no toca; el direccionamiento de `sort` (7.03× Rust) y `array` (4.88× Rust), después de lo anterior, porque el chequeo de límites no explica el grueso de `array`.

El compilador de la tabla se identifica como Copihue 0.22.3. El mismo generador, rebasado sobre main 0.23.0, es la versión 0.23.1. Los ocho `.asm` de este bench en `-O2`, quitada la línea `; Generated by Copihue …`, coinciden con los que emite 0.23.1. En `-O0` coinciden con 0.23.0. Los binarios `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode`, coinciden con los de 0.23.0 en `-O0` y en `-O2`.
