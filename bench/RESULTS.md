# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-25 16:06:58 UTC y 2026-10-25 16:07:36 UTC. Este archivo se escribió el 2026-10-25 16:07:36 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-25 16:06:58 UTC — 2026-10-25 16:07:36 UTC |
| Tablas escritas | 2026-10-25 16:07:36 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.26.1, flag `-O2` |
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
- Copihue `-O2` en hosted pliega constantes, reescribe `*`, `/` y `%` por potencias de dos, evalúa una operación de operandos simples directo en el registro del local (o en `rax`/`rbx` si hace falta un temporal), mantiene hasta cuatro locales escalares en `r12`–`r15` y, cuando ninguna llamada cae dentro del rango de uso, dos más en `r10` y `r11`, direcciona un array con `base+índice*escala` en una instrucción, y si la función abre con `if (param <op> hoja) return param` compara el registro de entrada y retorna antes de armar el frame. Una llamada directa del mismo programa, con a lo más seis argumentos que quepan en `rdi`–`r9`, los deja en esos registros; `syscall`, los builtins y un argumento que es otra llamada siguen con `push`/`pop`. Un `if` o `while` cuya condición es una comparación de escalares, o de un elemento de array, salta con las banderas y no materializa 0/1; `&&` y `||` también, y `&&` no evalúa la derecha si la izquierda ya falla. Esa carga se reutiliza en el `a[i]` o `a[i±constante] =` siguiente. Un `while (i < lim)` que arranca en una constante no negativa, solo suma uno a `i` al final, no llama y no toma direcciones, omite el chequeo de límites del array local cuando el arreglo tiene al menos `lim` elementos. El producto `s = s + a[i*n+k] * b[k*n+j]` con `k` de 0 a `n` camina las dos filas con punteros si `i` y `j` están en rango; si no, corre el bucle original. Una función que no usa `rbp` y no empuja guarda los registros con `push`/`pop`. No borra bucles. `-O0`, `--freestanding`, `--flat` y `--boot` no usan eso.
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 7.7 (7.2–8.5) | 46.1 (45.8–47.1) | 102.4 (100.5–129.5) | 68.1 (67.4–69.1) |
| `sieve` | sieve limit=400000 reps=80 | 8.7 (8.1–9.2) | 32.1 (31.6–33.6) | 104.6 (101.9–110.5) | 72.5 (68.8–76.0) |
| `matmul` | matmul N=384 | 9.8 (9.2–10.7) | 35.7 (35.2–37.0) | 110.5 (105.0–116.3) | 72.4 (71.2–73.9) |
| `arith` | arith n=100000000 | 8.1 (7.8–8.6) | 28.7 (28.1–29.2) | 105.7 (100.7–119.9) | 68.1 (66.8–69.9) |
| `chain` | chain n=80000000 | 8.8 (7.9–9.0) | 28.2 (28.1–30.4) | 102.4 (100.6–108.1) | 67.9 (67.3–69.0) |
| `fold` | fold n=500000000 | 8.4 (8.1–9.0) | 27.6 (27.3–28.3) | 99.8 (95.6–101.6) | 67.9 (66.9–70.3) |
| `array` | array n=100000 rounds=1000 | 8.4 (8.0–8.8) | 31.5 (30.9–32.4) | 106.5 (103.3–120.2) | 70.2 (68.4–72.3) |
| `sort` | sort n=28000 | 8.7 (8.5–10.2) | 31.8 (31.5–34.8) | 106.4 (105.4–109.4) | 69.6 (68.3–70.6) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9624 (385/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10232 (707/21/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10488 (1028/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (435/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (406/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9616 (386/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 9912 (588/1/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 10104 (701/41/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 394.3 (393.5–395.2) | 131.1 (130.2–131.5) | 194.1 (193.7–194.3) | 366.2 (365.9–366.8) | 3.01 | 2.03 | 1.08 |
| `sieve` | 6458901531 | 82.3 (82.1–82.5) | 70.5 (70.3–71.5) | 43.5 (43.2–43.7) | 128.8 (128.1–133.8) | 1.17 | 1.89 | 0.64 |
| `matmul` | 693633024 | 46.8 (46.4–47.1) | 32.5 (32.1–34.5) | 50.9 (50.6–51.2) | 78.7 (78.2–80.2) | 1.44 | 0.92 | 0.59 |
| `arith` | 3218085318 | 67.6 (66.9–71.6) | 59.6 (58.9–62.0) | 56.1 (55.5–56.4) | 67.1 (66.5–68.5) | 1.13 | 1.20 | 1.01 |
| `chain` | 215544461 | 166.7 (166.4–167.8) | 187.9 (187.6–188.2) | 182.8 (182.4–183.6) | 187.9 (187.8–188.4) | 0.89 | 0.91 | 0.89 |
| `fold` | 374999999750000000 | 142.1 (141.8–142.7) | 1.9 (1.5–3.3) | 1.8 (1.7–2.2) | 240.6 (226.2–246.3) | 74.09 | 77.14 | 0.59 |
| `array` | 2557452942256 | 57.5 (57.0–57.8) | 47.8 (44.2–53.6) | 34.4 (33.4–34.5) | 59.5 (59.1–59.8) | 1.20 | 1.67 | 0.97 |
| `sort` | 1070217027633 | 78.3 (78.1–78.4) | 73.6 (73.2–73.9) | 53.3 (53.1–53.7) | 154.3 (153.8–154.7) | 1.06 | 1.47 | 0.51 |

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
	linux-vdso.so.1 (0x00007f6f6d451000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f6f6d200000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f6f6d453000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f4bb4cf7000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f4bb4c64000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f4bb4a00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f4bb4cf9000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007fb9f5f3b000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fb9f5d81000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fb9f5a00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fb9f5f3d000)
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

`bench/src/array_global.lap` es el mismo algoritmo que `array`, con el arreglo en una global. Una carga de un array local emite un chequeo de límites, salvo el `while (i < lim)` de la metodología: el bucle interno de `array` y su suma final son de esa forma, así que no lo emiten. La global no lo emite. La salida coincidió con `array`.

| Variante | Mediana (mín–máx) ms |
|---|---:|
| local (`array`) | 57.5 (57.0–57.8) |
| global (`array_global`) | 45.6 (45.4–45.9) |

## Lectura de esta corrida

El cociente Copihue/C más alto es `fold` (74.09×) y el más bajo es `chain` (0.89×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En `fold` el decimal del resultado está en el ensamblado de gcc y de rustc, y el tiempo de esos dos es el de arrancar el proceso; OCaml sí ejecuta el bucle y ahí el cociente es el de la tabla (Copihue/OCaml). `chain` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone. El hueco que queda, en los bucles que los cuatro ejecutan, está en `sort`, `array`, `fib`, `matmul` y `arith`.

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
