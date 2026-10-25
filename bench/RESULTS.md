# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-25 13:56:57 UTC y 2026-10-25 13:57:43 UTC. Este archivo se escribió el 2026-10-25 14:06:29 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-25 13:56:57 UTC — 2026-10-25 13:57:43 UTC |
| Tablas escritas | 2026-10-25 14:06:29 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.22.3, flag `-O2` |
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
- Copihue `-O2` en hosted pliega constantes, reescribe `*`, `/` y `%` por potencias de dos, evalúa el lado derecho simple de una operación en `rbx` (sin `push`/`pop`) y mantiene hasta cuatro locales escalares en `r12`–`r15`. No simplifica bucles. `-O0`, `--freestanding`, `--flat` y `--boot` no usan eso.
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 8.5 (8.1–9.1) | 45.5 (44.8–48.6) | 118.5 (104.4–136.8) | 69.0 (66.4–69.7) |
| `sieve` | sieve limit=400000 reps=80 | 8.5 (8.1–9.8) | 31.3 (31.0–33.0) | 104.0 (98.4–107.5) | 66.9 (65.7–68.2) |
| `matmul` | matmul N=384 | 8.7 (8.3–9.9) | 34.4 (34.1–35.4) | 107.4 (101.6–112.2) | 71.8 (71.3–78.5) |
| `arith` | arith n=100000000 | 8.2 (7.7–8.3) | 27.6 (27.1–28.5) | 99.9 (96.7–103.3) | 68.9 (66.7–70.5) |
| `chain` | chain n=80000000 | 8.2 (7.8–9.4) | 27.7 (27.4–29.4) | 101.8 (98.5–106.2) | 68.8 (67.9–71.6) |
| `fold` | fold n=500000000 | 8.1 (7.5–9.2) | 27.3 (27.0–28.0) | 106.8 (103.9–140.7) | 70.3 (65.7–80.0) |
| `array` | array n=100000 rounds=1000 | 8.7 (8.2–10.2) | 30.9 (30.4–44.7) | 118.5 (100.3–144.2) | 68.5 (66.4–72.1) |
| `sort` | sort n=28000 | 9.1 (8.7–10.4) | 31.0 (30.3–32.9) | 104.2 (100.1–113.2) | 70.0 (67.7–72.2) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9648 (427/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10416 (993/41/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10520 (1258/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (509/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (470/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9616 (436/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 10184 (926/41/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 10528 (1110/81/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 456.7 (456.1–459.3) | 131.3 (130.3–133.5) | 194.2 (193.7–195.5) | 366.5 (365.9–368.0) | 3.48 | 2.35 | 1.25 |
| `sieve` | 6458901531 | 147.7 (146.7–148.5) | 70.5 (69.9–71.1) | 43.5 (42.9–44.1) | 128.6 (128.4–129.3) | 2.10 | 3.39 | 1.15 |
| `matmul` | 693633024 | 115.4 (114.5–125.8) | 33.2 (32.7–35.7) | 51.8 (51.1–52.3) | 79.0 (78.4–79.8) | 3.48 | 2.23 | 1.46 |
| `arith` | 3218085318 | 130.9 (130.6–132.6) | 59.8 (59.3–60.1) | 56.4 (55.6–56.8) | 67.6 (67.4–67.9) | 2.19 | 2.32 | 1.94 |
| `chain` | 215544461 | 169.1 (168.9–169.8) | 188.8 (188.0–189.7) | 184.6 (182.8–187.6) | 188.6 (187.6–191.0) | 0.90 | 0.92 | 0.90 |
| `fold` | 374999999750000000 | 419.9 (419.8–422.8) | 2.2 (1.6–2.6) | 1.9 (1.7–2.4) | 234.2 (213.6–242.0) | 187.68 | 216.90 | 1.79 |
| `array` | 2557452942256 | 165.0 (164.3–165.3) | 42.9 (42.0–43.7) | 33.8 (33.3–34.4) | 59.1 (58.7–59.5) | 3.84 | 4.88 | 2.79 |
| `sort` | 1070217027633 | 377.6 (376.9–377.7) | 74.1 (73.1–75.7) | 53.7 (53.1–54.3) | 154.0 (153.7–154.3) | 5.09 | 7.03 | 2.45 |

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
	linux-vdso.so.1 (0x00007f040cfdc000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f040cc00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f040cfde000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f4e9b839000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f4e9b7a6000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f4e9b400000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f4e9b83b000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007fbaeb0db000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fbaeaf21000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fbaeac00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fbaeb0dd000)
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
| local (`array`) | 165.0 (164.3–165.3) |
| global (`array_global`) | 156.4 (156.2–157.7) |

## Lectura de esta corrida

El cociente Copihue/C más alto es `fold` (187.68×) y el más bajo es `chain` (0.90×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En `fold` el decimal del resultado está en el ensamblado de gcc y de rustc, y el tiempo de esos dos es el de arrancar el proceso; OCaml sí ejecuta el bucle y ahí el cociente es el de la tabla (Copihue/OCaml). `chain` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone. El hueco que queda, en los bucles que los cuatro ejecutan, está en `sort`, `array`, `fib`, `matmul` y `arith`.

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
