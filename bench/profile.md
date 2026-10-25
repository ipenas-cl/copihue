## Perfil del código generado

El perfil es un recuento estático del ensamblado `-O2`. No es un perfil de ciclos: `perf` no está instalado en esta máquina. Una instrucción cuenta una vez por aparición. No se contaron comentarios, líneas en blanco ni etiquetas. `mov rbx, rax` es el paso de la máquina de pila. Una carga es un uso de `[rbp…]` que no es el destino. Un store es `mov [rbp…]`, `mov byte [rbp…]` o `mov qword [rbp…]`.

El bloque `Lbounds_error_*` (8 instrucciones, `syscall` de salida) no está en el camino: los índices de estos programas caen dentro del arreglo. El `jmp` al bloque `Lbounds_ok` sí corre, y entra en la cuenta. El `jmp` de vuelta a la cabeza del bucle también entra.

El bucle que se cuenta es el más interno de cada programa. En `sort` es una vuelta del desplazamiento en que `j >= 0` y `a[j] > key` salen verdaderas; cuántas vueltas da depende de los datos, y no se multiplica. En la criba es `while (j < limit) { flags[j] = 0; j = j + i; }`. `fib` cuenta el camino `n >= 2` desde la entrada de `fib` hasta el primer `ret` de ese camino, sin el bloque del caso base. No se contaron invocaciones en ejecución.

La columna 0.23.1 es el compilador de la corrida de las 14:15 UTC. Los ocho binarios `-O2` de este bench, compilados con 0.24.0 (el padre de este cambio), coinciden byte a byte con los de 0.23.1. La columna 0.24.1 es el compilador de la tabla de esta página.

| Programa | Bucle contado | 0.23.1 ins (push / pop / `mov rbx,rax` / cargas / stores / `call`) | 0.24.1 |
|---|---|---|---|
| `arith` | `.L0` + `jmp` | 30 (0 / 0 / 0 / 1 / 0 / 0) | 10 (0 / 0 / 0 / 0 / 0 / 0) |
| `fold` | `.L0` + `jmp` | 20 (0 / 0 / 0 / 0 / 0 / 0) | 7 (0 / 0 / 0 / 0 / 0 / 0) |
| `chain` | `.L0` + `jmp` | 30 (0 / 0 / 0 / 0 / 0 / 0) | 11 (0 / 0 / 0 / 0 / 0 / 0) |
| `array` | cuerpo interno, índice dentro de rango | 39 (1 / 1 / 2 / 2 / 2 / 0) | 17 (0 / 0 / 0 / 1 / 1 / 0) |
| `matmul` | bucle `k`, dos índices dentro de rango | 48 (2 / 2 / 4 / 3 / 1 / 0) | 32 (2 / 2 / 2 / 2 / 0 / 0) |
| `sieve` | marcado `flags[j] = 0` | 18 (1 / 1 / 1 / 0 / 1 / 0) | 6 (0 / 0 / 0 / 0 / 1 / 0) |
| `sort` | una vuelta del desplazamiento | 48 (1 / 1 / 3 / 2 / 1 / 0) | 34 (1 / 1 / 0 / 2 / 1 / 0) |
| `fib` | cuerpo con `n >= 2`, hasta el primer `ret` | 31 (4 / 3 / 1 / 1 / 1 / 2) | 24 (4 / 3 / 1 / 1 / 1 / 2) |

En `arith` 0.24.1 el bucle es `cmp r14, r10; jge; add r15, r12; and r15, 2147483647; add r12, r13; and r12, 2147483647; add r13, r14; and r13, 2147483647; add r14, 1; jmp`. `n` está en `r10`. No hay carga. `a`, `b`, `c` e `i` siguen en `r15`, `r12`, `r13` y `r14`.

En `fold` queda `imul rax, r12, 3; add r13, rax; add r13, 1`. En `chain`, `imul r12, r12, 13` y el desplazamiento `sar rax, 7; xor r12, rax`. En `array`, `v` se lee con `mov r10, [rbp-800016+r12*8]` y se escribe con un solo `mov qword [rbp-800016+r12*8], rax`. El `test`/`cmp`/`jmp` del límite sigue en el camino. En `sieve`, el marcado es `mov byte [rbp-3200016+r13*1], al`. En `matmul` el índice es `i*n+k`, no un nombre suelto, así que el `push`/`pop` de ese producto sigue; la carga ya es `[rbp+rax*8]`. En `sort`, `a[j]` es `[rbp-224008+r13*8]`. `a[j+1]` no es un nombre suelto: `mov rax, r13; add rax, 1; pop rbx; mov qword [base+rax*8], rbx`. `j >= 0` está dentro de `&&`, así que no es un salto directo del `while`.

En `fib`, `n` sigue en `r12` y sobrevive a los dos `call`. El primer resultado sigue en la pila. El caso `n < 2` es `cmp rdi, 2; jl; mov rax, rdi; ret`, antes de `push rbp` y antes de guardar `r12`. El cuerpo `n >= 2` sigue guardando `r12`.

`text` según `size`, bytes, 0.23.1 → 0.24.1: `fib` 427 → 395, `sieve` 993 → 787, `matmul` 1258 → 932, `arith` 509 → 435, `chain` 470 → 406, `fold` 436 → 386, `array` 926 → 698, `sort` 1110 → 884.

## Antes y después, mismo `make bench`

Dos corridas distintas, la misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld que la tabla de arriba. Copihue 0.23.1: muestras 2026-10-25 14:15:13 UTC — 14:15:59 UTC. Copihue 0.24.1: muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC, que son las de la tabla de esta página. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.23.1 / 0.24.1.

| Programa | 0.23.1 | 0.24.1 | 0.23.1 / 0.24.1 |
|---|---:|---:|---:|
| `fib` | 457.2 (456.0–462.0) | 389.7 (389.4–393.9) | 1.17 |
| `sieve` | 146.6 (145.8–152.2) | 93.0 (92.3–94.2) | 1.58 |
| `matmul` | 114.9 (114.0–120.8) | 78.8 (78.4–89.5) | 1.46 |
| `arith` | 130.8 (130.6–131.2) | 66.8 (66.5–67.4) | 1.96 |
| `chain` | 168.2 (168.0–168.8) | 166.8 (166.3–167.0) | 1.01 |
| `fold` | 419.2 (418.9–419.8) | 141.5 (141.3–142.0) | 2.96 |
| `array` | 164.8 (164.3–165.0) | 66.5 (66.4–67.0) | 2.48 |
| `sort` | 377.6 (377.2–379.3) | 255.4 (255.2–257.9) | 1.48 |

Las ocho medianas de Copihue bajaron. Entre las dos ventanas, C, Rust y OCaml se movieron unos pocos milisegundos (en `fib`, C pasó de 131.5 a 130.7). La bajada de Copihue es más grande que ese movimiento en `fib`, `sieve`, `matmul`, `arith`, `fold`, `array` y `sort`. En `chain` la bajada es 1.4 ms (168.2 → 166.8); los rangos de Copihue no se solapan entre sí (168.0–168.8 y 166.3–167.0), y en las dos ventanas Copihue queda por debajo de C, de Rust y de OCaml con rangos que tampoco se solapan. Eso es esta fila y estas corridas.

En la ventana 0.24.1, contra los otros tres, con rangos que no se solapan: `sieve` queda por debajo de OCaml (92.3–94.2 contra 128.0–129.9) y por encima de C y de Rust. `fold` queda por debajo de OCaml (141.3–142.0 contra 229.5–253.4). `chain` queda por debajo de los tres. `fib` queda por encima de los tres, también de OCaml (389.4–393.9 contra 365.4–369.2). `array` y `sort` quedan por encima de los tres. `arith` se solapa con OCaml (66.5–67.4 contra 66.5–67.3; el cociente de medianas es 0.99) y queda por encima de C y de Rust. `matmul` se solapa con OCaml (78.4–89.5 contra 78.5–83.0; el cociente es 1.00) y queda por encima de C y de Rust.

`fold` sigue lejos de gcc y de rustc (1.5 ms y 1.8 ms). Esos dos tienen el decimal del resultado en el `.s` y no ejecutaron el bucle. Este cambio no borra bucles.

En `array`, el local tardó 66.5 ms (66.4–67.0) y el global 45.5 ms (45.3–45.6). Los rangos no se solapan. La diferencia es 21.0 ms. En la corrida 0.23.1 era 164.8 contra 156.6 (156.4–157.0), 8.2 ms. El direccionamiento bajó las dos variantes; el chequeo de límites del local es una parte mayor de lo que queda, y no es todo el tiempo.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode`, generan el mismo código que 0.24.0. En hosted `-O0` el ensamblado de estos ocho programas coincide con 0.24.0 salvo la línea de versión.
