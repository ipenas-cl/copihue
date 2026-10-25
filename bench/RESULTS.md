# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-25 20:33:00 UTC y 2026-10-25 20:33:36 UTC. Este archivo se escribió el 2026-10-25 20:36:58 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-25 20:33:00 UTC — 2026-10-25 20:33:36 UTC |
| Tablas escritas | 2026-10-25 20:36:58 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.27.4, flag `-O2` |
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
- Copihue `-O2` en hosted pliega constantes, reescribe `*`, `/` y `%` por potencias de dos, evalúa una operación de operandos simples directo en el registro del local (o en `rax`/`rbx` si hace falta un temporal), mantiene hasta cuatro locales escalares en `r12`–`r15` y, cuando ninguna llamada cae dentro del rango de uso, dos más en `r10` y `r11`, direcciona un array con `base+índice*escala` en una instrucción, y si la función abre con `if (param <op> hoja) return param` compara el registro de entrada y retorna antes de armar el frame. Una llamada directa del mismo programa, con a lo más seis argumentos que quepan en `rdi`–`r9`, los deja en esos registros; `syscall`, los builtins y un argumento que es otra llamada siguen con `push`/`pop`. Un `if` o `while` cuya condición es una comparación de escalares, o de un elemento de array, salta con las banderas y no materializa 0/1; `&&` y `||` también, y `&&` no evalúa la derecha si la izquierda ya falla. Esa carga se reutiliza en el `a[i]` o `a[i±constante] =` siguiente. Un `while (i < lim)` que arranca en una constante no negativa, solo suma uno a `i` al final, no llama y no toma direcciones, omite el chequeo de límites del array local cuando el arreglo tiene al menos `lim` elementos. El producto `s = s + a[i*n+k] * b[k*n+j]` con `k` de 0 a `n` camina las dos filas con punteros si `i` y `j` están en rango; si no, corre el bucle original. Una función que no usa `rbp` y no empuja guarda los registros con `push`/`pop`. Un store de una constante que cabe en el elemento es un `mov` inmediato. Un llenado `a[i] = C` de un array `u8`/`i8`, con `i` desde 0 y el límite dentro del largo, es `rep stosb`. Una suma `s = s + A*i + B` con `i` de una constante a otra, paso 1, se sustituye por el total solo si cada producto y cada suma parcial cabe en i64 y los términos no cambian de signo; si puede envolver, el bucle se ejecuta. Una multiplicación de un registro por 3, 5 o 9 es un `lea`. Una racha de al menos dos `x = (x + y) & imm`, con `imm` entre 0 y 2147483647 y sin que `x` ni `y` se hayan escrito antes en la racha, emite los `add` y después los `and`. No invierte el `while` (medido, no bajó `arith`) y no desenrolla el producto interno (medido, no bajó `matmul`). No borra un bucle arbitrario y no reescribe `fib`. `-O0`, `--freestanding`, `--flat` y `--boot` no usan eso.
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 7.3 (6.7–7.5) | 43.9 (43.4–46.0) | 104.4 (100.9–105.8) | 66.6 (64.1–68.0) |
| `sieve` | sieve limit=400000 reps=80 | 8.0 (7.7–8.9) | 30.6 (30.2–31.7) | 111.0 (109.6–111.8) | 71.7 (69.8–75.9) |
| `matmul` | matmul N=384 | 9.1 (8.7–10.5) | 35.6 (34.6–36.5) | 114.5 (110.6–136.2) | 70.6 (69.8–71.3) |
| `arith` | arith n=100000000 | 7.2 (6.7–7.8) | 26.8 (26.1–27.9) | 104.0 (101.6–105.4) | 68.1 (66.2–68.2) |
| `chain` | chain n=80000000 | 7.5 (7.3–8.5) | 27.3 (26.2–28.8) | 104.3 (101.8–106.4) | 67.2 (65.1–67.9) |
| `fold` | fold n=500000000 | 7.4 (7.1–8.2) | 25.8 (24.9–27.3) | 100.7 (97.7–103.3) | 66.3 (65.4–68.8) |
| `array` | array n=100000 rounds=1000 | 7.8 (7.4–8.1) | 30.1 (29.2–31.4) | 111.5 (107.9–117.5) | 67.9 (67.4–68.6) |
| `sort` | sort n=28000 | 7.6 (7.3–8.4) | 30.6 (29.3–31.4) | 107.5 (107.0–111.3) | 68.2 (67.0–70.3) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9624 (385/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10136 (682/21/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10488 (1028/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (435/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (406/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9520 (380/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 9912 (588/1/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 10104 (701/41/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 393.9 (393.2–400.4) | 131.2 (130.5–132.2) | 194.1 (193.8–195.4) | 366.3 (366.1–367.2) | 3.00 | 2.03 | 1.08 |
| `sieve` | 6458901531 | 70.0 (69.5–70.7) | 70.2 (70.1–70.7) | 43.3 (43.0–43.7) | 128.4 (128.2–128.9) | 1.00 | 1.61 | 0.54 |
| `matmul` | 693633024 | 46.6 (46.2–47.2) | 31.6 (31.5–32.2) | 50.9 (50.7–51.7) | 81.0 (78.7–82.9) | 1.47 | 0.92 | 0.58 |
| `arith` | 3218085318 | 63.5 (63.3–63.8) | 59.5 (59.1–59.7) | 55.8 (55.6–56.2) | 66.7 (66.2–69.0) | 1.07 | 1.14 | 0.95 |
| `chain` | 215544461 | 166.7 (166.3–167.1) | 187.6 (187.2–188.0) | 182.5 (182.2–182.9) | 188.2 (187.4–188.5) | 0.89 | 0.91 | 0.89 |
| `fold` | 374999999750000000 | 1.4 (1.3–2.1) | 1.6 (1.2–1.9) | 1.3 (1.3–1.3) | 233.8 (224.0–245.7) | 0.88 | 1.08 | 0.01 |
| `array` | 2557452942256 | 45.6 (45.3–46.0) | 47.8 (43.1–49.8) | 33.7 (33.2–34.5) | 58.8 (58.6–59.2) | 0.95 | 1.35 | 0.78 |
| `sort` | 1070217027633 | 78.0 (77.8–78.9) | 73.5 (73.0–73.9) | 53.2 (53.1–53.4) | 154.3 (153.9–154.7) | 1.06 | 1.46 | 0.51 |

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
	linux-vdso.so.1 (0x00007f995a05c000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9959e00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f995a05e000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007fef5957c000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007fef594e9000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fef59200000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fef5957e000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f0e59d70000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f0e59bb6000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f0e59800000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f0e59d72000)
```

## `fold` y el bucle que otros compiladores pueden borrar

La salida común es `374999999750000000`.

Se buscó ese decimal en el ensamblado (`gcc -O2 -S`, `rustc -O --emit=asm`, `ocamlopt -O3 -S`, `copihue -O2 -S`):

| Compilador | El decimal aparece en el `.s` |
|---|---|
| gcc | sí |
| rustc | sí |
| ocamlopt | no |
| copihue | sí |

Un bucle de 500000000 vueltas no cabe en unos pocos milisegundos a ninguna frecuencia plausible de esta máquina (a 1 ciclo por vuelta y 2700.000 MHz harían falta unos 185 ms). Quien tiene el decimal en el `.s` no ejecutó el bucle: el trabajo se hizo al compilar. Quien no lo tiene hay que leerlo en la tabla de tiempos, no suponerlo.

## Array local y array global, solo Copihue

`bench/src/array_global.lap` es el mismo algoritmo que `array`, con el arreglo en una global. Una carga de un array local emite un chequeo de límites, salvo el `while (i < lim)` de la metodología: el bucle interno de `array` y su suma final son de esa forma, así que no lo emiten. La global no lo emite. La salida coincidió con `array`.

| Variante | Mediana (mín–máx) ms |
|---|---:|
| local (`array`) | 45.6 (45.3–46.0) |
| global (`array_global`) | 45.8 (45.0–46.0) |

## Lectura de esta corrida

El cociente Copihue/C más alto es `fib` (3.00×) y el más bajo es `fold` (0.88×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En `fold` la tabla del decimal dice gcc sí, rustc sí, Copihue sí y OCaml no. Quien lo tiene no ejecutó las 500000000 vueltas, y ese tiempo es el de arrancar el proceso. Quien no lo tiene sí ejecuta el bucle; el cociente Copihue/OCaml es el de la tabla. `chain` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone. El hueco que queda, en los bucles que los cuatro ejecutan, está en `fib`, `sieve`, `matmul`, `arith`, `array` y `sort`.

En tiempo de compilación de estos archivos chicos, la mediana de Copihue es la más baja en las ocho filas. Eso mide el arranque de un compilador de una pasada más nasm y ld, no un proyecto grande.

El fichero de Copihue es el más chico de cada fila. `file` marca los binarios de Rust y de OCaml con información de depuración; la columna `text` de `size` es el código, y también ahí Copihue es el más chico. El binario de C está enlazado con libc y el de Copihue no: el tamaño no es "bytes del algoritmo".

La mediana del array local y la del global están en la tabla de arriba. El hueco que quede entre las dos está medido ahí; el perfil de más abajo cuenta las instrucciones del bucle interno.

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

Misma máquina y las mismas versiones de gcc, rustc, ocamlopt, nasm y ld. La referencia es la ventana 2026-10-25 17:30:43 UTC — 17:31:18 UTC. Ese binario se identificaba como 0.27.2. El ensamblado `-O2` de los ocho programas coincide con 0.27.3 salvo la línea de versión. La tabla de tiempos de esta página es 0.27.4, muestras 2026-10-25 20:33:00 UTC — 20:33:36 UTC. Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es la referencia / esta medición.

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

Se midió dar dos pasos por salto en el producto con `n` par, en la ventana 2026-10-25 17:40:12 UTC — 17:40:48 UTC. `matmul` pasó de 46.5 ms (45.9–47.1) a 47.6 ms (47.4–48.1). Los rangos no se solapan, y el segundo es más lento. Ese binario también tenía el `lea` del llenado. El desenrollado no está en 0.27.4. La ventana de esta página es sin él.

`-O0`, `--freestanding`, `--flat` y `--boot`, incluido `tests/usermode` y `tests/ctx`, generan el mismo código que 0.27.3, salvo la línea de versión. `bits 16` y `bits 32` no entran en estas reescrituras.
