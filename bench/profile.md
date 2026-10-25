## Perfil del código generado

El perfil es un recuento estático del ensamblado `-O2`. No es un perfil de ciclos: `perf` no está instalado en esta máquina. Una instrucción cuenta una vez por aparición. No se contaron comentarios, líneas en blanco ni etiquetas. `mov rbx, rax` es el paso de la máquina de pila. Una carga es un uso de `[rbp…]` o de `[rdi]`/`[rsi]` que no es el destino. Un store es `mov [rbp…]`, `mov byte [rbp…]` o `mov qword [rbp…]`.

El bloque `Lbounds_error_*` (8 instrucciones, `syscall` de salida) no está en el camino: los índices de estos programas caen dentro del arreglo. En 0.25.1 el `jmp` al bloque `Lbounds_ok` sí corría y entraba en la cuenta. En 0.26.2 ese `jmp` ya no está: el camino dentro de rango cae al cuerpo, y el `jge` hacia el error sí entra en la cuenta porque se ejecuta. El `jmp` de vuelta a la cabeza del bucle también entra.

El bucle que se cuenta es el más interno de cada programa. En `sort` es una vuelta del desplazamiento en que `j >= 0` y `a[j] > key` salen verdaderas. En la criba es `while (j < limit) { flags[j] = 0; j = j + i; }`. `fib` cuenta el camino `n >= 2` desde la entrada de `fib` hasta el primer `ret` de ese camino, sin el bloque del caso base. No se contaron invocaciones en ejecución.

Los ocho binarios `-O2` de este bench, compilados con 0.26.0, coinciden byte a byte con los de 0.25.1. La columna 0.25.1 es esa corrida (muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC). La columna 0.26.2 es la ventana 2026-10-25 16:06:58 UTC — 16:07:36 UTC. La tabla de tiempos de esta página es la medición de 0.27.1 (el binario de esa ventana se identificaba como 0.26.3).

| Programa | Bucle contado | 0.25.1 ins (push / pop / `mov rbx,rax` / cargas / stores / `call`) | 0.26.2 |
|---|---|---|---|
| `arith` | `.L0` + `jmp` | 10 (0 / 0 / 0 / 0 / 0 / 0) | 10, el mismo binario |
| `fold` | `.L0` + `jmp` | 7 (0 / 0 / 0 / 0 / 0 / 0) | 7, el mismo binario |
| `chain` | `.L0` + `jmp` | 11 (0 / 0 / 0 / 0 / 0 / 0) | 11, el mismo binario |
| `array` | cuerpo interno, índice dentro de rango | 17 (0 / 0 / 0 / 1 / 1 / 0) | 12 (0 / 0 / 0 / 1 / 1 / 0) |
| `matmul` | bucle `k`, dos índices dentro de rango | 32 (2 / 2 / 2 / 2 / 0 / 0) | 10 (0 / 0 / 0 / 2 / 0 / 0) |
| `sieve` | marcado `flags[j] = 0` | 6 (0 / 0 / 0 / 0 / 1 / 0) | 6 (0 / 0 / 0 / 0 / 1 / 0) |
| `sort` | una vuelta del desplazamiento | 34 (1 / 1 / 0 / 2 / 1 / 0) | 10 (0 / 0 / 0 / 1 / 1 / 0) |
| `fib` | cuerpo con `n >= 2`, hasta el primer `ret` | 24 (4 / 3 / 1 / 1 / 1 / 2) | 16 (2 / 2 / 0 / 0 / 0 / 2) |

En `array` 0.26.2 el cuerpo es `cmp r12, r13; jge; mov r10, [rbp-800016+r12*8]; mov rbx, r12; and rbx, 7; imul rax, r10, 3; add rax, rbx; add rax, 1; and rax, 1023; mov qword [rbp-800016+r12*8], rax; add r12, 1; jmp`. El `test`/`js`/`cmp`/`jge` del límite ya no está: el `while` anterior es `i = 0` y el índice es `i`, con `n` igual a la longitud. La suma final tampoco lo emite. El bucle de llenado ya no lo emitía, porque es un store.

En `sort` la vuelta tomada es `cmp r13, 0; jl; cmp r13, 28000; jge .Lbounds_error; mov rax, [rbp-224008+r13*8]; cmp rax, r15; jle; mov qword [rbp-224000+r13*8], rax; sub r13, 1; jmp`. Diez instrucciones. `j >= 0 && a[j] > key` salta sin pasar por 0/1, y `a[j] >= 0` quita el `test`/`js` de la carga. El store de `a[j+1]` reutiliza `rax` y usa la dirección `base+(j+1)*8`. El tope `j < 28000` sigue. `key = a[i]` sigue con el chequeo completo, porque ese índice no es el `i` de un `while (i < lim)` probado.

En `matmul` el `k` tomado es `cmp r15, r14; jge .Lend; mov rax, [rdi]; mov rbx, [rsi]; imul rax, rbx; add r10, rax; add rdi, 8; add rsi, rcx; add r15, 1; jmp`. Diez instrucciones. Antes de entrar, ocho instrucciones comprueban `i` y `j` en `[0, n)` y arman los dos punteros (`lea rdi`, `lea rsi`, `shl rcx, 3`). Si la guarda falla, corre el bucle de 0.25.1, con sus chequeos. En este bench `i` y `j` están en rango, así que esa copia no corre. `text` pasa de 932 a 1028 bytes por esa copia.

En `sieve` el marcado sigue en seis instrucciones: `cmp r13, r14; jge; mov rax, 0; mov byte [rbp-3200016+r13*1], al; add r13, r12; jmp`. Lo que cambió es el alrededor. `if (flags[i] != 0)` es `movzx; cmp rax, 0; je`, y la suma `while (i < limit)` ya no chequea el límite. `i * i < limit` sigue materializando 0/1, porque el lado izquierdo no es una hoja.

En `fib` el camino `n >= 2` es `cmp rdi, 2; jl .Lbase_fib; push r13; push r12; sub rsp, 8; mov r12, rdi; lea rdi, [r12-1]; call fib; mov r13, rax; lea rdi, [r12-2]; call fib; add rax, r13; add rsp, 8; pop r12; pop r13; ret`. Dieciséis instrucciones. Los dos argumentos van en `rdi`. El primer resultado queda en `r13`, que es callee-saved, y las dos llamadas entran con `rsp` alineado a 16. `main` hace `mov rdi, 40; call fib`. El caso `n < 2` sigue siendo `mov rax, rdi; ret` antes de los `push`.

`text` según `size`, bytes, 0.25.1 → 0.26.2: `fib` 395 → 385, `sieve` 787 → 707, `matmul` 932 → 1028, `arith` 435 → 435, `chain` 406 → 406, `fold` 386 → 386, `array` 698 → 588, `sort` 884 → 701. `matmul` crece. Los otros tres que cambiaron bajan.

## Antes y después, mismo `make bench`

Dos corridas distintas, la misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld que la tabla de arriba. Copihue 0.25.1: muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC. Copihue 0.26.2: muestras 2026-10-25 16:06:58 UTC — 16:07:36 UTC. Esa ventana no es la tabla de tiempos de esta página; la tabla de arriba es la medición de 0.27.1. En la ventana 0.26.2 el binario se identificaba como 0.26.1; main ya usó ese número, y el release es 0.26.2. Los ocho binarios `-O2` de 0.26.2 coinciden con los medidos ahí. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.25.1 / 0.26.2.

| Programa | 0.25.1 | 0.26.2 | 0.25.1 / 0.26.2 |
|---|---:|---:|---:|
| `fib` | 389.7 (389.4–393.9) | 394.3 (393.5–395.2) | 0.99 |
| `sieve` | 93.0 (92.3–94.2) | 82.3 (82.1–82.5) | 1.13 |
| `matmul` | 78.8 (78.4–89.5) | 46.8 (46.4–47.1) | 1.68 |
| `arith` | 66.8 (66.5–67.4) | 67.6 (66.9–71.6) | 0.99 |
| `chain` | 166.8 (166.3–167.0) | 166.7 (166.4–167.8) | 1.00 |
| `fold` | 141.5 (141.3–142.0) | 142.1 (141.8–142.7) | 1.00 |
| `array` | 66.5 (66.4–67.0) | 57.5 (57.0–57.8) | 1.16 |
| `sort` | 255.4 (255.2–257.9) | 78.3 (78.1–78.4) | 3.26 |

`sort`, `matmul`, `array` y `sieve` bajaron, y los rangos de Copihue no se solapan entre las dos ventanas. `arith`, `chain` y `fold` son el mismo binario que en 0.25.1; las medianas se solapan (66.9–67.4, 166.4–167.0 y 141.8–142.0) y no hay una mejora medida. `fib` tiene menos instrucciones en el camino `n >= 2` (24 → 16) y la mediana no bajó: 394.3 contra 389.7. Los rangos se solapan en 393.5–393.9. El tiempo se lo llevan las dos llamadas.

En la ventana 0.26.2, con rangos que no se solapan: `sort` queda por debajo de OCaml (78.1–78.4 contra 153.8–154.7) y por encima de C (73.2–73.9) y de Rust (53.1–53.7). El cociente contra Rust pasa de 4.80 a 1.47, y contra C de 3.46 a 1.06. `matmul` queda por debajo de Rust (46.4–47.1 contra 50.6–51.2) y de OCaml (78.2–80.2), y por encima de C (32.1–34.5). `array` queda por debajo de OCaml (57.0–57.8 contra 59.1–59.8) y por encima de C (44.2–53.6) y de Rust (33.4–34.5). `sieve` queda por debajo de OCaml (82.1–82.5 contra 128.1–133.8) y por encima de C y de Rust. `fold` queda por debajo de OCaml (141.8–142.7 contra 226.2–246.3). `chain` queda por debajo de los tres (166.4–167.8 contra C 187.6–188.2, Rust 182.4–183.6 y OCaml 187.8–188.4); el binario es el de 0.25.1, y esto es esta fila y estas corridas. `fib` queda por encima de los tres, también de OCaml (393.5–395.2 contra 365.9–366.8); el cociente contra C es 3.01. `arith` se solapa con OCaml (66.9–71.6 contra 66.5–68.5; el cociente de medianas es 1.01) y queda por encima de C y de Rust.

`fold` sigue lejos de gcc y de rustc (1.9 ms y 1.8 ms). Esos dos tienen el decimal del resultado en el `.s` y no ejecutaron el bucle. Este cambio no borra bucles.

En `array`, el local tardó 57.5 ms (57.0–57.8) y el global 45.6 ms (45.4–45.9). Los rangos no se solapan. La diferencia es 11.9 ms. En la corrida 0.25.1 era 66.5 contra 45.5 (45.3–45.6), 21.0 ms. El global casi no se movió. Quitar el chequeo del bucle interno bajó el local; no explica todo el hueco que queda.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode`, generan el mismo código que 0.26.1. En hosted `-O0` el ensamblado de estos ocho programas coincide con 0.26.1 salvo la línea de versión. `bits 16` y `bits 32` no entran en estas reescrituras.

## 0.26.2 → 0.26.3

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. Copihue 0.26.2: muestras 2026-10-25 16:06:58 UTC — 16:07:36 UTC. La medición de este cambio: muestras 2026-10-25 17:00:28 UTC — 17:01:04 UTC, que son las de la tabla de esta página. En esa ventana el binario se identificaba como 0.26.3. main ya usó 0.27.0 para los structs, y el release de este cambio es 0.27.1. El ensamblado `-O2` de los ocho programas coincide con el medido, salvo la línea de versión. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.26.2 / esta medición.

| Programa | 0.26.2 | 0.26.3 | 0.26.2 / 0.26.3 |
|---|---:|---:|---:|
| `fib` | 394.3 (393.5–395.2) | 393.2 (392.8–393.4) | 1.00 |
| `sieve` | 82.3 (82.1–82.5) | 69.6 (69.4–70.0) | 1.18 |
| `matmul` | 46.8 (46.4–47.1) | 46.5 (46.4–48.5) | 1.01 |
| `arith` | 67.6 (66.9–71.6) | 67.0 (66.8–67.3) | 1.01 |
| `chain` | 166.7 (166.4–167.8) | 166.6 (166.3–168.8) | 1.00 |
| `fold` | 142.1 (141.8–142.7) | 1.6 (1.1–2.5) | 88.81 |
| `array` | 57.5 (57.0–57.8) | 57.0 (56.7–57.6) | 1.01 |
| `sort` | 78.3 (78.1–78.4) | 78.0 (77.7–78.2) | 1.00 |

`arith`, `fib`, `matmul`, `chain`, `array` y `sort` son el mismo binario que en 0.26.2. Sus medianas se solapan, salvo `fib`: 392.8–393.4 contra 393.5–395.2, un hueco de 0.1 ms entre ventanas. El código no cambió; no es una mejora de este compilador. `sieve` y `fold` sí cambiaron, y sus rangos no se solapan con los de 0.26.2.

El bucle contado es el mismo de la tabla de arriba. En 0.26.3:

| Programa | 0.26.2 ins | 0.26.3 |
|---|---|---|
| `arith` | 10 | 10, el mismo binario |
| `fold` | 7 | no hay bucle: dos `mov` del total |
| `chain` | 11 | 11, el mismo binario |
| `array` | 12 | 12, el mismo binario |
| `matmul` | 10 | 10, el mismo binario |
| `sieve` | 6 | 5 (0 / 0 / 0 / 0 / 1 / 0) |
| `sort` | 10 | 10, el mismo binario |
| `fib` | 16 | 16, el mismo binario |

En `sieve` el marcado es `cmp r13, r14; jge; mov byte [rbp-3200016+r13*1], 0; add r13, r12; jmp`. Cinco instrucciones: el `mov rax, 0` desapareció. El llenado de `flags` ya no es un bucle por elemento. Es `lea rdi, [rbp-3200016]; mov rcx, 400000; mov al, 1; rep stosb`, y después `mov byte [rbp-3200016], 0` y `mov byte [rbp-3200015], 0`. `i * i < limit` sigue materializando 0/1 y el `imul` de afuera se repite: no hay eliminación de esa subexpresión. El bucle de marcado no se desenrolló.

En `fold` el cuerpo es `mov r13, 374999999750000000; mov r12, 500000000`. No hay `imul` ni salto de vuelta. El decimal está en el `.s`, igual que en gcc y en rustc. Una suma que puede envolver, un coeficiente con los dos signos, o un `i * i`, conserva el bucle.

`text` según `size`, bytes, 0.26.2 → 0.26.3: `sieve` 707 → 682, `fold` 386 → 380. Los otros seis no cambian (`fib` 385, `matmul` 1028, `arith` 435, `chain` 406, `array` 588, `sort` 701).

En la ventana 0.26.3, con rangos que no se solapan salvo donde se dice: `fold` queda en 1.6 ms (1.1–2.5) y se solapa con C (1.3–1.9) y con Rust (1.3–2.1). No queda por debajo de esos dos; el resto es arrancar el proceso. Queda por debajo de OCaml (211.4–256.3). `sieve` baja de 82.3 ms a 69.6 ms (69.4–70.0). Contra C (70.0–70.4) los rangos se tocan en 70.0; el cociente de medianas es 0.99 y no se puede decir que queda por debajo de C. Sigue por encima de Rust (42.8–43.6; cociente 1.62) y por debajo de OCaml (127.8–128.6). `fib` sigue en 3.00× C (392.8–393.4 contra 130.5–131.2), por encima de Rust (193.4–194.0) y de OCaml (365.3–366.0). El prólogo de `n >= 2` ya era de 16 instrucciones y no se tocó. `arith` se solapa con OCaml (66.8–67.3 contra 66.3–66.9) y queda por encima de C (59.2–59.8) y de Rust (55.2–56.1). `matmul` queda por debajo de Rust (46.4–48.5 contra 50.2–51.0) y de OCaml (77.6–78.5), y por encima de C (31.8–32.6). `array` queda por debajo de OCaml (56.7–57.6 contra 58.3–59.2) y por encima de C (42.0–49.7) y de Rust (32.9–35.6). `sort` queda por debajo de OCaml (77.7–78.2 contra 153.6–154.4) y por encima de C (73.2–73.9) y de Rust (52.9–53.3). `chain` queda por debajo de los tres (166.3–168.8 contra C 187.3–187.6, Rust 182.1–183.0 y OCaml 187.2–189.7). El binario es el de 0.25.1, y esto es esta fila y estas corridas.

Se midió invertir el `while` (un chequeo al entrar y un salto tomado al volver) en la ventana 2026-10-25 16:52:01 UTC — 16:52:37 UTC. En `arith` la mediana pasó de 67.6 ms (66.9–71.6) a 73.4 ms (70.7–74.6). Los rangos se tocan en 70.7–71.6. `fib` no se movió. Esa inversión no está en 0.26.3. La ventana de arriba es sin ella.

No se reescribió el árbol de llamadas de `fib`. No hay inlining: en estos ocho programas la función caliente no llama a otra función pequeña no recursiva dentro del bucle (`print_int` queda fuera; `fib` es recursiva). No se desenrolló el marcado ni `arith`: el paso `j = j + i` no tiene un resto demostrado sin desborde. Un módulo por una constante que no es potencia de dos sigue en `idiv`; `*` `/` `%` por potencias de dos ya estaban en 0.26.2.

En `array`, el local tardó 57.0 ms (56.7–57.6) y el global 45.2 ms (45.0–45.5). Los rangos no se solapan. La diferencia es 11.8 ms. El binario de `array` es el de 0.26.2.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode`, generan el mismo código que 0.26.2, salvo la línea de versión. `bits 16` y `bits 32` no entran en estas reescrituras.
