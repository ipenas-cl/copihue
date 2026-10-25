## Perfil del código generado

El perfil es un recuento estático del ensamblado `-O2`. No es un perfil de ciclos: `perf` no está instalado en esta máquina. Una instrucción cuenta una vez por aparición. No se contaron comentarios, líneas en blanco ni etiquetas. `mov rbx, rax` es el paso de la máquina de pila. Una carga es un uso de `[rbp…]` o de `[rdi]`/`[rsi]` que no es el destino. Un store es `mov [rbp…]`, `mov byte [rbp…]` o `mov qword [rbp…]`.

El bloque `Lbounds_error_*` (8 instrucciones, `syscall` de salida) no está en el camino: los índices de estos programas caen dentro del arreglo. En 0.25.1 el `jmp` al bloque `Lbounds_ok` sí corría y entraba en la cuenta. En 0.26.2 ese `jmp` ya no está: el camino dentro de rango cae al cuerpo, y el `jge` hacia el error sí entra en la cuenta porque se ejecuta. El `jmp` de vuelta a la cabeza del bucle también entra.

El bucle que se cuenta es el más interno de cada programa. En `sort` es una vuelta del desplazamiento en que `j >= 0` y `a[j] > key` salen verdaderas. En la criba es `while (j < limit) { flags[j] = 0; j = j + i; }`. `fib` cuenta el camino `n >= 2` desde la entrada de `fib` hasta el primer `ret` de ese camino, sin el bloque del caso base. No se contaron invocaciones en ejecución.

Los ocho binarios `-O2` de este bench, compilados con 0.26.0, coinciden byte a byte con los de 0.25.1. La columna 0.25.1 es esa corrida (muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC). La columna 0.26.2 es la ventana 2026-10-25 16:06:58 UTC — 16:07:36 UTC. La medición publicada de 0.27.2 es la ventana 2026-10-25 17:00:28 UTC — 17:01:04 UTC (el binario se identificaba como 0.26.3). No es la tabla de tiempos de esta página.

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

Dos corridas distintas, la misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld que la tabla de arriba. Copihue 0.25.1: muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC. Copihue 0.26.2: muestras 2026-10-25 16:06:58 UTC — 16:07:36 UTC. Esa ventana no es la tabla de tiempos de esta página. La medición de 0.27.2 está en la sección 0.26.2 → 0.26.3. En la ventana 0.26.2 el binario se identificaba como 0.26.1; main ya usó ese número, y el release es 0.26.2. Los ocho binarios `-O2` de 0.26.2 coinciden con los medidos ahí. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.25.1 / 0.26.2.

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

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. Copihue 0.26.2: muestras 2026-10-25 16:06:58 UTC — 16:07:36 UTC. La medición de 0.27.2: muestras 2026-10-25 17:00:28 UTC — 17:01:04 UTC. No es la tabla de esta página. En esa ventana el binario se identificaba como 0.26.3. main ya usó 0.27.0 para los structs y 0.27.1 para el cambio de contexto, y el release de este cambio es 0.27.2. El ensamblado `-O2` de los ocho programas coincide con el medido, salvo la línea de versión. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.26.2 / esta medición.

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

## 0.27.3 → 0.27.4

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. La referencia es la ventana 2026-10-25 17:30:43 UTC — 17:31:18 UTC. Ese binario se identificaba como 0.27.2. El ensamblado `-O2` de los ocho programas coincide con 0.27.3 salvo la línea de versión. La medición de 0.27.4 es la ventana 2026-10-25 20:33:00 UTC — 20:33:36 UTC. No es la tabla de tiempos de esta página. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es la referencia / esta medición.

| Programa | 17:30 (0.27.2) | 0.27.4 | referencia / 0.27.4 |
|---|---:|---:|---:|
| `fib` | 393.0 (392.6–394.3) | 393.9 (393.2–400.4) | 1.00 |
| `sieve` | 69.2 (68.7–69.7) | 70.0 (69.5–70.7) | 0.99 |
| `matmul` | 46.5 (45.9–47.1) | 46.6 (46.2–47.2) | 1.00 |
| `arith` | 66.6 (66.4–67.1) | 63.5 (63.3–63.8) | 1.05 |
| `chain` | 166.4 (166.1–166.8) | 166.7 (166.3–167.1) | 1.00 |
| `fold` | 1.5 (1.0–2.0) | 1.4 (1.3–2.1) | 1.07 |
| `array` | 57.1 (56.9–57.5) | 45.6 (45.3–46.0) | 1.25 |
| `sort` | 77.9 (77.7–78.4) | 78.0 (77.8–78.9) | 1.00 |

`array` y `arith` bajan, y los rangos no se solapan con la referencia. Los otros seis se solapan. `fib`, `sieve`, `chain`, `fold` y `sort` son el mismo binario que en 0.27.3. `matmul` solo cambia el llenado: `imul rax, r12, 3` pasa a `lea rax, [r12+r12*2]`. El bucle `k` sigue en 10 instrucciones. `text` no cambia (`fib` 385, `sieve` 682, `matmul` 1028, `arith` 435, `chain` 406, `fold` 380, `array` 588, `sort` 701).

En `array` el cuerpo interno sigue siendo 12 instrucciones. `imul rax, r10, 3` es `lea rax, [r10+r10*2]`, y el llenado `i * 3 + 1` es el mismo `lea`. El checksum `a[i] * (i + 1)` sigue en el camino de `push`/`pop`. En esta ventana el local queda en 45.6 ms (45.3–46.0) y el global en 45.8 ms (45.0–46.0). Los rangos se solapan: no queda un hueco medido entre los dos. En la referencia el local era 57.1 ms (56.9–57.5) y el global 45.2 ms (45.0–45.7); el global se solapa con esa referencia.

En `arith` el bucle sigue en 10 instrucciones y sigue probado por arriba (`cmp`/`jge` y un `jmp` tomado). El cuerpo es `add r15, r12; add r12, r13; add r13, r14; and r15, 2147483647; and r12, 2147483647; and r13, 2147483647; add r14, 1`. Las tres sumas usan los valores de antes de la máscara: `b` no es destino de `a`, ni `c` de `b`, ni `i` de `c`.

En la ventana 20:33, con rangos que no se solapan salvo donde se dice: `array` queda por debajo de OCaml (45.3–46.0 contra 58.6–59.2). Contra C (43.1–49.8) los rangos se solapan; el cociente de medianas es 0.95 y no se puede decir que queda por debajo de C. Sigue por encima de Rust (33.2–34.5; cociente 1.35). `arith` queda por debajo de OCaml (63.3–63.8 contra 66.2–69.0) y por encima de C (59.1–59.7) y de Rust (55.6–56.2). `matmul` queda por debajo de Rust (46.2–47.2 contra 50.7–51.7) y de OCaml (78.7–82.9), y por encima de C (31.5–32.2). El tiempo se solapa con la referencia: esta ronda no lo bajó. `fib` sigue en 3.00× C (393.2–400.4 contra 130.5–132.2), por encima de Rust (193.8–195.4) y de OCaml (366.1–367.2). El prólogo de `n >= 2` sigue en 16 instrucciones. gcc `-O2` reescribe la recursión; eso no se hizo. `sort` queda por debajo de OCaml (77.8–78.9 contra 153.9–154.7) y por encima de C (73.0–73.9) y de Rust (53.1–53.4). El desplazamiento sigue con el chequeo de límite superior. `sieve` se solapa con C (69.5–70.7 contra 70.1–70.7); el cociente de medianas es 1.00. Sigue por encima de Rust (43.0–43.7) y por debajo de OCaml (128.2–128.9). `fold` queda en 1.4 ms (1.3–2.1) y se solapa con C (1.2–1.9) y con Rust (1.3–1.3). No queda por debajo de esos dos. Queda por debajo de OCaml (224.0–245.7). `chain` queda por debajo de los tres (166.3–167.1 contra C 187.2–188.0, Rust 182.2–182.9 y OCaml 187.4–188.5). El binario es el de 0.25.1, y esto es esta fila y estas corridas.

Se midió dar dos pasos por salto en el producto con `n` par, en la ventana 2026-10-25 17:40:12 UTC — 17:40:48 UTC. `matmul` pasó de 46.5 ms (45.9–47.1) a 47.6 ms (47.4–48.1). Los rangos no se solapan, y el segundo es más lento. Ese binario también tenía el `lea` del llenado. El desenrollado no está en 0.27.4. La ventana 20:33 es sin él.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode` y `tests/ctx`, generan el mismo código que 0.27.3, salvo la línea de versión. `bits 16` y `bits 32` no entran en estas reescrituras.

## 0.27.4 → desplazamiento (binario 0.27.5)

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. La referencia es la ventana 2026-10-25 20:44:22 UTC — 20:44:58 UTC, Copihue 0.27.4. Esta medición es 2026-10-25 21:11:18 UTC — 2026-10-25 21:11:53 UTC. Ese binario imprimía 0.27.5. No es la tabla de tiempos de esta página. La tabla es la ventana 2026-10-25 21:45:10 UTC — 2026-10-25 21:45:46 UTC. Ese binario imprimía 0.28.2. `main` ya usaba 0.29.0 para `-c`, así que la versión de esta ronda es 0.29.1. Los ocho programas `-O2` coinciden con ese ensamblado salvo la línea de versión. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es la referencia / esta medición.

| Programa | 20:44 (0.27.4) | 21:11 (0.27.5) | referencia / 21:11 |
|---|---:|---:|---:|
| `fib` | 394.4 (393.5–416.2) | 393.2 (392.8–395.0) | 1.00 |
| `sieve` | 69.6 (69.2–70.1) | 69.7 (69.2–70.0) | 1.00 |
| `matmul` | 46.9 (46.3–47.4) | 46.2 (46.1–46.7) | 1.02 |
| `arith` | 63.5 (62.9–64.3) | 63.3 (63.1–63.6) | 1.00 |
| `chain` | 166.6 (166.2–167.0) | 166.6 (166.0–168.0) | 1.00 |
| `fold` | 1.4 (1.2–2.0) | 1.3 (1.0–1.7) | 1.08 |
| `array` | 45.5 (45.3–46.0) | 45.5 (45.3–45.8) | 1.00 |
| `sort` | 77.9 (77.6–78.4) | 52.6 (52.4–55.3) | 1.48 |

`sort` baja, y los rangos no se solapan con la referencia (55.3 contra 77.6). Los otros siete se solapan. `fib`, `sieve`, `matmul`, `arith`, `chain`, `fold` y `array` son el mismo binario que en 0.27.4, salvo la línea de versión. `text` no cambia en esos siete (`fib` 385, `sieve` 682, `matmul` 1028, `arith` 435, `chain` 406, `fold` 380, `array` 588). `sort` pasa de 701 a 659 bytes de `text`, y `data` de 41 a 21: sobra una copia de la cadena del chequeo.

En 0.27.4 una vuelta del desplazamiento eran 10 instrucciones: `cmp`/`jl`, `cmp`/`jge` contra 28000, la carga, `cmp`/`jle`, el store, `sub` y `jmp`. En ese binario, y en 0.29.1, el camino tomado es

```
mov qword [rbp-224000+r13*8], rax
sub r13, 1
js .L7
mov rax, [rbp-224008+r13*8]
cmp rax, r15
jg .L8
```

Seis instrucciones. `j` entra en `i - 1`. Con `i` en `[0, 27999]` eso es a lo más 27998, y solo resta 1 después de `j >= 0`, así que el único negativo es -1 y `js` sale antes de leer `a[-1]`. El store de `a[j + 1]` con `j == -1` cae en `a[0]`. `key = a[i]` sigue con su chequeo, una vez por vuelta exterior. gcc `-O2` tiene la misma forma (store, `sub`, `je`, carga, `cmp`, `jg` tomado) y tampoco chequea el tope.

En la ventana 21:11, con rangos que no se solapan salvo donde se dice: `sort` queda por debajo de C (52.4–55.3 contra 72.9–73.5) y de OCaml (153.3–154.6). Contra Rust (52.6–53.6) los rangos se solapan; el cociente de medianas es 0.99 y no se puede decir que queda por debajo de Rust. `matmul` queda por debajo de Rust (46.1–46.7 contra 50.1–51.0) y de OCaml (77.7–80.2), y por encima de C (31.6–32.2). El tiempo se solapa con la referencia: el bucle `k` es el de 0.27.4. `fib` sigue en 3.00× C (392.8–395.0 contra 130.5–130.9), por encima de Rust (193.2–194.1) y de OCaml (365.3–366.3). El prólogo de `n >= 2` sigue en 16 instrucciones. gcc `-O2` escribe 416 líneas para `fib.c`, con `sub rsp, 200` y un árbol que resta 2. Sigue siendo exponencial (130.9 ms, no microsegundos). Esa reescritura no está en 0.29.1. `arith` queda por debajo de OCaml (63.1–63.6 contra 66.1–66.9) y por encima de C (58.9–59.4) y de Rust (55.1–56.1). `array` queda por debajo de OCaml (45.3–45.8 contra 58.5–59.2) y por encima de C (41.5–43.2) y de Rust (32.9–34.2). El local y el global se solapan (45.3–45.8 y 45.1–46.1). `sieve` se solapa con C (69.2–70.0 contra 69.8–70.3); el cociente de medianas es 0.99. Sigue por encima de Rust (42.8–43.4) y por debajo de OCaml (127.6–128.3). `fold` queda en 1.3 ms (1.0–1.7) y se solapa con C (1.1–1.6) y con Rust (1.3–2.0). No queda por debajo de esos dos. Queda por debajo de OCaml (223.7–250.5). `chain` queda por debajo de los tres (166.0–168.0 contra C 187.0–188.0, Rust 182.0–183.2 y OCaml 187.3–187.9). El binario es el de 0.25.1, y esto es esta fila y estas corridas.

Se midió probar el bucle `k` al final y multiplicar con `imul rax, qword [rsi]`, en la ventana 2026-10-25 21:06:42 UTC — 2026-10-25 21:07:17 UTC. `matmul` pasó de 46.9 ms (46.3–47.4) a 48.0 ms (47.6–48.2). Los rangos no se solapan, y el segundo es más lento. `text` de ese binario era 1021. El bucle de 10 instrucciones, probado arriba, es el de esta página. El desenrollado par tampoco está.

Ese binario, en `-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode` y `tests/ctx`, genera el mismo código que 0.28.0, salvo la línea de versión. `bits 16` y `bits 32` no entran en el desplazamiento.

## 21:11 → 0.29.1

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. La referencia es la ventana 21:11 de arriba (el binario imprimía 0.27.5). La tabla de tiempos de esta página es la ventana 2026-10-25 21:45:10 UTC — 2026-10-25 21:45:46 UTC. Ese binario imprimía 0.28.2. `main` ya usaba 0.29.0 para `-c`, así que la versión es 0.29.1, y los ocho programas `-O2` coinciden con esa medición salvo la línea de versión. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es la referencia / esta medición.

| Programa | 21:11 (0.27.5) | 0.29.1 | referencia / 0.29.1 |
|---|---:|---:|---:|
| `fib` | 393.2 (392.8–395.0) | 393.8 (393.0–394.8) | 1.00 |
| `sieve` | 69.7 (69.2–70.0) | 72.3 (71.9–72.5) | 0.96 |
| `matmul` | 46.2 (46.1–46.7) | 47.9 (46.4–48.6) | 0.96 |
| `arith` | 63.3 (63.1–63.6) | 63.6 (63.1–63.9) | 1.00 |
| `chain` | 166.6 (166.0–168.0) | 166.8 (166.4–168.6) | 1.00 |
| `fold` | 1.3 (1.0–1.7) | 1.5 (1.3–2.5) | 0.87 |
| `array` | 45.5 (45.3–45.8) | 45.2 (45.1–45.5) | 1.01 |
| `sort` | 52.6 (52.4–55.3) | 52.6 (52.4–53.1) | 1.00 |

`sieve` sube, y los rangos no se solapan (71.9 contra 70.0). `matmul` se solapa con 21:11 (46.4–46.7). Los otros seis se solapan y el ensamblado `-O2` es el de 21:11, salvo la línea de versión. `text` de esos seis no cambia (`fib` 385, `arith` 435, `chain` 406, `fold` 380, `array` 588, `sort` 659). `sieve` pasa de 682 a 672 y `matmul` de 1028 a 1018.

En `sieve` lo que cambia es `while (i * i < limit)`, no el marcado. Antes: `cmp rax, rbx; setl al; movzx rax, al; test rax, rax; jz`. Ahora: `cmp rax, rbx; jge`. El marcado sigue en seis instrucciones. En `matmul` lo que cambia es la suma `while (i < n * n)`, la misma secuencia `setl` / `movzx` / `test` / `jz` reemplazada por `jge`. El bucle `k` sigue en 10 instrucciones, probado arriba.

En la ventana 21:45, con rangos que no se solapan salvo donde se dice: `sort` queda por debajo de C (52.4–53.1 contra 72.9–73.7) y de OCaml (153.7–154.3). Contra Rust (52.6–53.6) los rangos se solapan; el cociente de medianas es 0.99. `sieve` se solapa con C (71.9–72.5 contra 69.9–72.4); el cociente de medianas es 1.03. Sigue por encima de Rust (42.7–43.3) y por debajo de OCaml (128.0–134.6). `matmul` queda por debajo de Rust (46.4–48.6 contra 50.3–51.3) y de OCaml (78.4–81.3), y por encima de C (32.0–34.0). El tiempo se solapa con 21:11. `fib` queda en 3.01× C (393.0–394.8 contra 130.8–138.4), por encima de Rust (193.5–194.7) y de OCaml (365.9–366.8). El prólogo de `n >= 2` sigue en 16 instrucciones. `arith` queda por debajo de OCaml (63.1–63.9 contra 66.3–70.0). Contra C (59.4–63.7) los rangos se solapan; el cociente de medianas es 1.07. Sigue por encima de Rust (55.4–56.0). `array` queda por debajo de OCaml (45.1–45.5 contra 58.6–59.1) y por encima de Rust (33.3–33.7). Contra C (42.2–77.6) los rangos se solapan; el cociente de medianas es 1.05. El local y el global se solapan (45.1–45.5 y 44.7–45.7). `fold` queda en 1.5 ms (1.3–2.5) y se solapa con C (1.2–1.7) y con Rust (1.6–2.0). Queda por debajo de OCaml (219.9–245.0). `chain` queda por debajo de los tres (166.4–168.6 contra C 187.0–189.5, Rust 182.0–182.9 y OCaml 186.9–188.0). El binario es el de 0.25.1, y esto es esta fila y estas corridas.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode` y `tests/ctx`, generan el mismo código que 0.29.0, salvo la línea de versión. `bits 16` y `bits 32` no entran en el desplazamiento.

## 0.29.1 → 0.29.3

`main` tomó 0.29.2 para el arreglo de `ebx` en `bits 16` y `bits 32`. Esta ronda es 0.29.3 y no cambia el generador de 64 bits. La ventana de esta página es 2026-10-25 22:33:23 UTC — 2026-10-25 22:34:10 UTC. El binario medido imprimía 0.29.2. El `-O2` hosted de los trece programas coincide con ese binario salvo la línea de versión. Los ocho programas de antes se solapan con la ventana 21:45 (el binario de aquella imprimía 0.28.2 y el `-O2` coincide con 0.29.1 salvo la versión): `fib` 393.3–404.6 contra 393.0–394.8, `sieve` 72.1–72.7 contra 71.9–72.5, `matmul` 46.2–46.5 contra 46.4–48.6, `arith` 62.8–63.4 contra 63.1–63.9, `chain` 165.9–166.6 contra 166.4–168.6, `fold` 0.9–2.3 contra 1.3–2.5, `array` 45.0–45.9 contra 45.1–45.5, `sort` 52.3–53.0 contra 52.4–53.1. `text` de esos ocho no cambia (`fib` 385, `sieve` 672, `matmul` 1018, `arith` 435, `chain` 406, `fold` 380, `array` 588, `sort` 659).

gcc `-O2` de `matmul.c` no intercambia `i, j, k`. El producto interno es SSE (`movdqa`, `pmuludq`, `paddq`) y el cero del resultado es `memset`. Eso no se emite. El desenrollado par y el `k` probado al final siguen fuera. gcc `-O2` de `fib.c` sigue en 416 líneas: árbol par/impar, `sub rsp, 200`, una `call fib`. No hay un `mov` de menos en el prólogo de la recurrencia directa. El prólogo de `n >= 2` sigue en 16 instrucciones. `fib` en esta ventana es 3.06× C (393.3–404.6 contra 130.4–131.8), por encima de Rust (193.3–194.1) y de OCaml (365.3–365.8).

Los cinco programas nuevos ejecutan el bucle: el decimal de la salida no está en ningún `.s`. Donde Copihue queda por encima, con rangos que no se solapan:

- `copy` es el hueco grande: 47.8 ms (47.5–49.0) contra C 9.6 (9.0–10.0), 4.97×. También por encima de Rust (16.8–17.4) y por debajo de OCaml (162.7–163.1). gcc emite `call memcpy`. El bucle de Copihue es `movzx rax, byte [src+r12*1]` / `mov byte [dst+r12*1], al` / `add` / `jmp`, seis instrucciones con el `cmp`. `text` 534. No hay `rep movs` de una copia.
- `fill` queda en 32.4 ms (32.1–32.7) contra C 11.9 (11.2–12.9), 2.72×, por encima de Rust (12.0–12.8) y por debajo de OCaml (91.4–91.9). gcc vectoriza con `movdqa`. El bucle de Copihue es escalar: `movzx`, dos `add`, `and`, `mov byte`, doce instrucciones con el `cmp` y el `jmp`. No es un llenado constante, así que no entra en `rep stosb`. `text` 493.
- `parse` queda en 22.3 ms (21.9–22.5) contra C 13.8 (13.2–14.2), 1.62×, por encima de Rust (12.8–16.1) y por debajo de OCaml (32.8–35.8). En el dígito, gcc hace `lea rdx, [rdx+rdx*4]` y `lea rdx, -48[rcx+rdx*2]`. Copihue hace `imul r13, r13, 10`. `text` 586.
- `list` queda por encima de los tres: 53.4 ms (53.0–54.2) contra C 33.3 (33.0–33.7), Rust 41.8 (41.4–42.1) y OCaml 30.9 (30.4–31.2). 1.61× C. Cada salto hace `imul rax, 16` dos veces, con `push`/`pop` del acumulador. gcc es `sal rax, 4` / `add` base / `add` valor / `mov` next / `test` / `jns`, seis instrucciones. OCaml guarda el par en un array plano; el tiempo de esa fila es ese layout. `text` 627.
- `hash` se solapa con C: 45.2 ms (44.6–45.4) contra 45.3 (45.1–45.7). El cociente de medianas es 1.00. Queda por encima de Rust (38.7–39.0) y por debajo de OCaml (46.0–47.0). Los dos bucles son un `imul` dependiente por byte (gcc seis instrucciones, con el puntero ya avanzado; Copihue doce, con `push`/`pop`). La latencia del `imul` tapa la diferencia contra gcc. `text` 494.

`sieve` en esta ventana queda por encima de C sin solape (72.1–72.7 contra 69.7–70.2); el rango de Copihue se solapa con el de 21:45, y el ensamblado es el de entonces. `arith` queda por encima de C (62.8–63.4 contra 58.9–59.7) y `array` también (45.0–45.9 contra 40.9–41.5). Los dos se solapan con 21:45. `matmul` sigue por encima de C (46.2–46.5 contra 31.4–32.1) y por debajo de Rust (50.0–50.7) y de OCaml (77.6–80.3). `sort` queda por debajo de C (52.3–53.0 contra 73.2–77.2) y de OCaml (153.2–157.7). Contra Rust (52.7–53.5) se solapa; el cociente de medianas es 0.99. El local de `array` y el global se solapan (45.0–45.9 y 45.0–45.4). `fold` se solapa con C (0.9–2.3 contra 1.2–2.2) y con Rust (1.3–1.6). Queda por debajo de OCaml (230.3–254.1). El decimal está en el `.s` de gcc, rustc y Copihue. `chain` queda por debajo de los tres (165.9–166.6 contra C 186.7–187.4, Rust 181.8–184.7 y OCaml 186.9–187.5). El binario de `chain` es el de 0.25.1, y esto es esta fila y estas corridas.

El hosted de 64 bits genera el mismo código que el binario medido (imprimía 0.29.2), salvo la línea de versión. `bits 16` y `bits 32` son el arreglo de `ebx` de 0.29.2.
