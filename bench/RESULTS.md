# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-25 15:01:14 UTC y 2026-10-25 15:01:54 UTC. Este archivo se escribió el 2026-10-25 15:10:47 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-25 15:01:14 UTC — 2026-10-25 15:01:54 UTC |
| Tablas escritas | 2026-10-25 15:10:47 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.25.1, flag `-O2` |
| C | gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0, flag `-O2`, -march=x86-64 |
| Rust | rustc 1.83.0 (90b35a623 2024-11-26), `-O` (Equivalent to -C opt-level=2) |
| OCaml | ocamlopt 4.14.1, flag `-O3`, flambda=false |
| nasm | NASM version 2.16.01 |
| ld | GNU ld (GNU Binutils for Ubuntu) 2.42 |

## Metodología

Cada programa está escrito cuatro veces (`.lap`, `.c`, `.rs`, `.ml`) con el mismo algoritmo, los mismos límites y la misma salida: un entero y un salto de línea. La primera línea de cada fuente lleva la carga (`BENCH:`); el script aborta si no coinciden. Antes de medir, las cuatro salidas se comparan byte a byte.

- Compilación: el comando entero hasta el ejecutable. En Copihue eso incluye `nasm -f elf64` y `ld`. En gcc, rustc y ocamlopt incluye su ensamblador y su enlazador. 5 repeticiones, mediana. No hay calentamiento aparte: la pasada de verificación ya compiló una vez, así que la primera repetición medida no es un arranque en frío del disco.
- Ejecución: 7 repeticiones más una de calentamiento que no entra en la mediana. La mediana es el valor central de las 7 muestras ordenadas (la 4.ª si son 7). En cada celda, entre paréntesis, el mínimo y el máximo de esas repeticiones.
- La ejecución medida tira stdout a `/dev/null`. El proceso sigue escribiendo una línea. Afinidad a un CPU: sí, `taskset -c 0`.
- El tamaño es `wc -c` del fichero producido, sin `strip`. `text`/`data`/`bss` salen de `size`.
- Copihue `-O2` en hosted pliega constantes, reescribe `*`, `/` y `%` por potencias de dos, evalúa una operación de operandos simples directo en el registro del local (o en `rax`/`rbx` si hace falta un temporal), mantiene hasta cuatro locales escalares en `r12`–`r15` y, cuando ninguna llamada cae dentro del rango de uso, dos más en `r10` y `r11`, direcciona un array con `base+índice*escala` en una instrucción, y si la función abre con `if (param <op> hoja) return param` compara el registro de entrada y retorna antes de armar el frame. No simplifica bucles. `-O0`, `--freestanding`, `--flat` y `--boot` no usan eso.
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 8.4 (7.7–8.7) | 44.8 (44.4–45.8) | 101.0 (96.6–120.0) | 67.4 (64.7–68.6) |
| `sieve` | sieve limit=400000 reps=80 | 8.5 (7.7–8.7) | 31.2 (31.1–31.8) | 100.1 (98.6–103.3) | 69.1 (67.9–70.0) |
| `matmul` | matmul N=384 | 8.8 (8.0–9.8) | 35.2 (33.7–37.4) | 105.3 (100.4–106.3) | 70.7 (70.4–70.9) |
| `arith` | arith n=100000000 | 8.2 (7.8–9.0) | 27.6 (27.2–28.4) | 96.5 (93.3–102.7) | 69.5 (66.7–71.1) |
| `chain` | chain n=80000000 | 8.3 (7.3–8.9) | 27.9 (27.2–28.4) | 94.6 (92.0–96.0) | 67.0 (66.1–68.0) |
| `fold` | fold n=500000000 | 8.2 (7.6–8.4) | 26.2 (26.1–26.8) | 92.3 (90.5–94.6) | 67.4 (66.7–69.4) |
| `array` | array n=100000 rounds=1000 | 8.4 (8.2–9.3) | 30.7 (30.0–31.1) | 99.2 (96.3–102.2) | 69.5 (67.7–69.8) |
| `sort` | sort n=28000 | 8.3 (8.1–9.4) | 31.3 (30.6–31.9) | 100.0 (98.7–101.7) | 68.4 (67.6–69.9) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9624 (395/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10416 (787/41/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10520 (932/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (435/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (406/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9616 (386/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 10184 (698/41/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 10528 (884/81/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 389.7 (389.4–393.9) | 130.7 (130.2–132.3) | 194.0 (193.4–194.6) | 365.8 (365.4–369.2) | 2.98 | 2.01 | 1.07 |
| `sieve` | 6458901531 | 93.0 (92.3–94.2) | 70.5 (70.2–70.8) | 43.5 (43.3–44.0) | 128.3 (128.0–129.9) | 1.32 | 2.14 | 0.72 |
| `matmul` | 693633024 | 78.8 (78.4–89.5) | 32.3 (32.1–32.7) | 51.8 (50.7–52.1) | 78.9 (78.5–83.0) | 2.44 | 1.52 | 1.00 |
| `arith` | 3218085318 | 66.8 (66.5–67.4) | 59.6 (59.4–62.5) | 55.8 (54.9–56.3) | 67.2 (66.5–67.3) | 1.12 | 1.20 | 0.99 |
| `chain` | 215544461 | 166.8 (166.3–167.0) | 187.4 (187.0–187.7) | 182.3 (182.1–182.9) | 187.9 (187.4–188.4) | 0.89 | 0.91 | 0.89 |
| `fold` | 374999999750000000 | 141.5 (141.3–142.0) | 1.5 (1.4–2.2) | 1.8 (1.6–2.1) | 243.3 (229.5–253.4) | 95.57 | 77.40 | 0.58 |
| `array` | 2557452942256 | 66.5 (66.4–67.0) | 46.9 (43.4–49.7) | 33.6 (33.1–33.9) | 59.2 (58.6–60.0) | 1.42 | 1.98 | 1.12 |
| `sort` | 1070217027633 | 255.4 (255.2–257.9) | 73.7 (73.4–74.0) | 53.2 (52.9–53.6) | 154.2 (153.8–155.1) | 3.46 | 4.80 | 1.66 |

## Enlace

`ldd` y `file` del binario de `fib` (los demás usan el mismo compilador):

### fib lap

```
/workspace/bench/out/bin/fib_lap: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, not stripped
	not a dynamic executable
```

### fib c

```
/workspace/bench/out/bin/fib_c: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=8c7fbbe3fe25a028fbbbcb1c2a779126601723cd, for GNU/Linux 3.2.0, not stripped
	linux-vdso.so.1 (0x00007ff98a21c000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ff989e00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007ff98a21e000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f9dfc2a2000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f9dfc20f000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9dfbe00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f9dfc2a4000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f0d3ecc9000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f0d3eb0f000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f0d3e800000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f0d3eccb000)
```

## `fold` y el bucle que otros compiladores pueden borrar

La salida común es `374999999750000000`.

Se buscó ese decimal en el ensamblado (`gcc -O2 -S`, `rustc -O --emit=asm`, `ocamlopt -O3 -S`):

| Compilador | El decimal aparece en el `.s` |
|---|---|
| gcc | sí |
| rustc | sí |
| ocamlopt | no |

Un bucle de 500000000 vueltas no cabe en unos pocos milisegundos a ninguna frecuencia plausible de esta máquina (a 1 ciclo por vuelta y 2700.000 MHz harían falta unos 185 ms). Si C o Rust están muy por debajo de eso, no ejecutaron el bucle: el trabajo se hizo al compilar. OCaml y Copihue hay que leerlos en la tabla, no suponerlo.

## Array local y array global, solo Copihue

`bench/src/array_global.lap` es el mismo algoritmo que `array`, con el arreglo en una global. En el generador actual, la lectura de un array local emite un chequeo de límites (`test`/`cmp`/`jmp`) y la de un array global no. La salida coincidió con `array`.

| Variante | Mediana (mín–máx) ms |
|---|---:|
| local (`array`) | 66.5 (66.4–67.0) |
| global (`array_global`) | 45.5 (45.3–45.6) |

## Lectura de esta corrida

El cociente Copihue/C más alto es `fold` (95.57×) y el más bajo es `chain` (0.89×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En `fold` el decimal del resultado está en el ensamblado de gcc y de rustc, y el tiempo de esos dos es el de arrancar el proceso; OCaml sí ejecuta el bucle y ahí el cociente es el de la tabla (Copihue/OCaml). `chain` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone. El hueco que queda, en los bucles que los cuatro ejecutan, está en `sort`, `array`, `fib`, `matmul` y `arith`.

En tiempo de compilación de estos archivos chicos, la mediana de Copihue es la más baja en las ocho filas. Eso mide el arranque de un compilador de una pasada más nasm y ld, no un proyecto grande.

El fichero de Copihue es el más chico de cada fila. `file` marca los binarios de Rust y de OCaml con información de depuración; la columna `text` de `size` es el código, y también ahí Copihue es el más chico. El binario de C está enlazado con libc y el de Copihue no: el tamaño no es "bytes del algoritmo".

La mediana del array local y la del global están en la tabla de arriba. La diferencia no es el grueso del tiempo de `array`: esta corrida no muestra que el chequeo de límites sea lo que domina ese programa.

## Límites

- Esto no mide C++. El C de la tabla es `gcc -O2`.
- El tamaño del fichero mezcla el código del programa con el runtime y el tipo de enlace. No es una comparación de "bytes de algoritmo".
- El tiempo de ejecución incluye crear el proceso, tocar la pila y escribir una línea. No es un bucle interno cronometrado con `rdtsc`.
- Máquina virtual. La mediana recorta un pico, no convierte el número en una cifra de hardware físico.
- No se midió código freestanding, ni un programa grande, ni el uso de memoria.
- Copihue reserva en la pila 8 bytes por elemento aunque el elemento sea `u8` (el hueco se redondea al número de elementos). La criba local de 400000 bytes lógicos ocupa unos 3.2 MB de marco. Cabe en el `ulimit -s` de esta corrida.
- Un `let` vive hasta el fin de su bloque. Declarar el mismo nombre dos veces en un bloque es un error. Los programas de esta suite declaran cada nombre una vez.

Los datos crudos, en nanosegundos, están en `bench/results.tsv`.
## Perfil del código generado

El perfil es un recuento estático del ensamblado `-O2`. No es un perfil de ciclos: `perf` no está instalado en esta máquina. Una instrucción cuenta una vez por aparición. No se contaron comentarios, líneas en blanco ni etiquetas. `mov rbx, rax` es el paso de la máquina de pila. Una carga es un uso de `[rbp…]` que no es el destino. Un store es `mov [rbp…]`, `mov byte [rbp…]` o `mov qword [rbp…]`.

El bloque `Lbounds_error_*` (8 instrucciones, `syscall` de salida) no está en el camino: los índices de estos programas caen dentro del arreglo. El `jmp` al bloque `Lbounds_ok` sí corre, y entra en la cuenta. El `jmp` de vuelta a la cabeza del bucle también entra.

El bucle que se cuenta es el más interno de cada programa. En `sort` es una vuelta del desplazamiento en que `j >= 0` y `a[j] > key` salen verdaderas; cuántas vueltas da depende de los datos, y no se multiplica. En la criba es `while (j < limit) { flags[j] = 0; j = j + i; }`. `fib` cuenta el camino `n >= 2` desde la entrada de `fib` hasta el primer `ret` de ese camino, sin el bloque del caso base. No se contaron invocaciones en ejecución.

La columna 0.23.1 es el compilador de la corrida de las 14:15 UTC. Los ocho binarios `-O2` de este bench, compilados con 0.25.0 (main al rebasar), coinciden byte a byte con los de 0.23.1 y con los de 0.24.0. La columna 0.25.1 es este generador: los ocho binarios coinciden con los medidos a las 15:01 UTC, cuando el mismo código se identificaba como 0.24.1, antes de rebasar sobre 0.25.0.

| Programa | Bucle contado | 0.23.1 ins (push / pop / `mov rbx,rax` / cargas / stores / `call`) | 0.25.1 |
|---|---|---|---|
| `arith` | `.L0` + `jmp` | 30 (0 / 0 / 0 / 1 / 0 / 0) | 10 (0 / 0 / 0 / 0 / 0 / 0) |
| `fold` | `.L0` + `jmp` | 20 (0 / 0 / 0 / 0 / 0 / 0) | 7 (0 / 0 / 0 / 0 / 0 / 0) |
| `chain` | `.L0` + `jmp` | 30 (0 / 0 / 0 / 0 / 0 / 0) | 11 (0 / 0 / 0 / 0 / 0 / 0) |
| `array` | cuerpo interno, índice dentro de rango | 39 (1 / 1 / 2 / 2 / 2 / 0) | 17 (0 / 0 / 0 / 1 / 1 / 0) |
| `matmul` | bucle `k`, dos índices dentro de rango | 48 (2 / 2 / 4 / 3 / 1 / 0) | 32 (2 / 2 / 2 / 2 / 0 / 0) |
| `sieve` | marcado `flags[j] = 0` | 18 (1 / 1 / 1 / 0 / 1 / 0) | 6 (0 / 0 / 0 / 0 / 1 / 0) |
| `sort` | una vuelta del desplazamiento | 48 (1 / 1 / 3 / 2 / 1 / 0) | 34 (1 / 1 / 0 / 2 / 1 / 0) |
| `fib` | cuerpo con `n >= 2`, hasta el primer `ret` | 31 (4 / 3 / 1 / 1 / 1 / 2) | 24 (4 / 3 / 1 / 1 / 1 / 2) |

En `arith` 0.25.1 el bucle es `cmp r14, r10; jge; add r15, r12; and r15, 2147483647; add r12, r13; and r12, 2147483647; add r13, r14; and r13, 2147483647; add r14, 1; jmp`. `n` está en `r10`. No hay carga. `a`, `b`, `c` e `i` siguen en `r15`, `r12`, `r13` y `r14`.

En `fold` queda `imul rax, r12, 3; add r13, rax; add r13, 1`. En `chain`, `imul r12, r12, 13` y el desplazamiento `sar rax, 7; xor r12, rax`. En `array`, `v` se lee con `mov r10, [rbp-800016+r12*8]` y se escribe con un solo `mov qword [rbp-800016+r12*8], rax`. El `test`/`cmp`/`jmp` del límite sigue en el camino. En `sieve`, el marcado es `mov byte [rbp-3200016+r13*1], al`. En `matmul` el índice es `i*n+k`, no un nombre suelto, así que el `push`/`pop` de ese producto sigue; la carga ya es `[rbp+rax*8]`. En `sort`, `a[j]` es `[rbp-224008+r13*8]`. `a[j+1]` no es un nombre suelto: `mov rax, r13; add rax, 1; pop rbx; mov qword [base+rax*8], rbx`. `j >= 0` está dentro de `&&`, así que no es un salto directo del `while`.

En `fib`, `n` sigue en `r12` y sobrevive a los dos `call`. El primer resultado sigue en la pila. El caso `n < 2` es `cmp rdi, 2; jl; mov rax, rdi; ret`, antes de `push rbp` y antes de guardar `r12`. El cuerpo `n >= 2` sigue guardando `r12`.

`text` según `size`, bytes, 0.23.1 → 0.25.1: `fib` 427 → 395, `sieve` 993 → 787, `matmul` 1258 → 932, `arith` 509 → 435, `chain` 470 → 406, `fold` 436 → 386, `array` 926 → 698, `sort` 1110 → 884.

## Antes y después, mismo `make bench`

Dos corridas distintas, la misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld que la tabla de arriba. Copihue 0.23.1: muestras 2026-10-25 14:15:13 UTC — 14:15:59 UTC. Copihue 0.25.1: muestras 2026-10-25 15:01:14 UTC — 15:01:54 UTC, que son las de la tabla de esta página. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es 0.23.1 / 0.25.1.

| Programa | 0.23.1 | 0.25.1 | 0.23.1 / 0.25.1 |
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

En la ventana 0.25.1, contra los otros tres, con rangos que no se solapan: `sieve` queda por debajo de OCaml (92.3–94.2 contra 128.0–129.9) y por encima de C y de Rust. `fold` queda por debajo de OCaml (141.3–142.0 contra 229.5–253.4). `chain` queda por debajo de los tres. `fib` queda por encima de los tres, también de OCaml (389.4–393.9 contra 365.4–369.2). `array` y `sort` quedan por encima de los tres. `arith` se solapa con OCaml (66.5–67.4 contra 66.5–67.3; el cociente de medianas es 0.99) y queda por encima de C y de Rust. `matmul` se solapa con OCaml (78.4–89.5 contra 78.5–83.0; el cociente es 1.00) y queda por encima de C y de Rust.

`fold` sigue lejos de gcc y de rustc (1.5 ms y 1.8 ms). Esos dos tienen el decimal del resultado en el `.s` y no ejecutaron el bucle. Este cambio no borra bucles.

En `array`, el local tardó 66.5 ms (66.4–67.0) y el global 45.5 ms (45.3–45.6). Los rangos no se solapan. La diferencia es 21.0 ms. En la corrida 0.23.1 era 164.8 contra 156.6 (156.4–157.0), 8.2 ms. El direccionamiento bajó las dos variantes; el chequeo de límites del local es una parte mayor de lo que queda, y no es todo el tiempo.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode`, generan el mismo código que 0.25.0. En hosted `-O0` el ensamblado de estos ocho programas coincide con 0.25.0 salvo la línea de versión.
