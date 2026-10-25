# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-25 12:50:52 UTC y 2026-10-25 12:51:48 UTC. Este archivo se escribió el 2026-10-25 12:55:38 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-25 12:50:52 UTC — 2026-10-25 12:51:48 UTC |
| Tablas escritas | 2026-10-25 12:55:38 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.19.0, flag `-O2` |
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
- Copihue `-O2` pliega constantes y reescribe `*`, `/` y `%` por potencias de dos. No hay asignación de registros ni simplificación de bucles. El código de una operación es el de una máquina de pila (`push`/`pop`).
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 7.5 (7.4–8.4) | 43.3 (42.4–46.7) | 99.5 (98.4–141.4) | 63.0 (61.3–67.1) |
| `sieve` | sieve limit=400000 reps=80 | 8.4 (7.8–8.5) | 30.7 (29.0–31.3) | 102.5 (100.8–107.5) | 68.6 (67.4–68.9) |
| `matmul` | matmul N=384 | 9.4 (8.5–9.6) | 35.0 (33.6–41.6) | 113.6 (108.5–116.5) | 72.4 (69.8–72.7) |
| `arith` | arith n=100000000 | 7.8 (7.4–8.2) | 27.5 (26.5–29.4) | 107.7 (103.7–111.0) | 69.8 (67.9–74.6) |
| `chain` | chain n=80000000 | 7.0 (6.6–8.1) | 27.4 (26.2–27.8) | 107.4 (103.1–110.0) | 70.2 (67.1–80.6) |
| `fold` | fold n=500000000 | 8.1 (7.4–10.2) | 26.7 (25.9–32.4) | 113.3 (107.7–136.6) | 73.1 (68.3–82.1) |
| `array` | array n=100000 rounds=1000 | 9.7 (9.2–9.9) | 32.8 (31.1–36.1) | 117.9 (111.6–123.8) | 71.9 (69.2–73.4) |
| `sort` | sort n=28000 | 9.1 (8.7–9.5) | 32.3 (30.6–36.5) | 115.8 (110.9–126.3) | 70.7 (68.7–72.5) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9648 (431/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10416 (1121/41/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10520 (1514/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (533/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (500/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9616 (436/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 10184 (1051/41/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 10528 (1246/81/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 745.6 (736.7–758.0) | 141.9 (133.7–214.4) | 199.1 (196.5–202.8) | 370.3 (369.3–373.4) | 5.25 | 3.74 | 2.01 |
| `sieve` | 6458901531 | 183.2 (180.9–227.6) | 72.3 (71.5–73.2) | 47.1 (45.7–50.6) | 130.6 (128.9–136.1) | 2.53 | 3.89 | 1.40 |
| `matmul` | 693633024 | 174.9 (168.0–184.4) | 35.5 (34.9–37.6) | 53.7 (51.6–54.9) | 82.7 (80.9–102.9) | 4.93 | 3.26 | 2.11 |
| `arith` | 3218085318 | 239.8 (238.0–248.3) | 60.4 (60.3–60.7) | 57.4 (56.6–60.4) | 80.5 (68.1–84.1) | 3.97 | 4.18 | 2.98 |
| `chain` | 215544461 | 206.2 (202.1–223.7) | 189.4 (188.2–191.5) | 183.7 (183.1–184.3) | 189.2 (189.0–190.0) | 1.09 | 1.12 | 1.09 |
| `fold` | 374999999750000000 | 751.2 (741.0–758.8) | 1.8 (1.5–2.7) | 1.7 (1.4–1.9) | 213.0 (210.1–219.8) | 422.75 | 432.02 | 3.53 |
| `array` | 2557452942256 | 264.5 (263.3–377.5) | 43.3 (42.7–47.1) | 35.0 (34.5–35.3) | 60.0 (59.1–62.6) | 6.10 | 7.56 | 4.41 |
| `sort` | 1070217027633 | 493.8 (488.7–501.9) | 75.2 (74.9–75.9) | 55.6 (55.5–56.6) | 158.9 (157.4–160.4) | 6.57 | 8.88 | 3.11 |

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
	linux-vdso.so.1 (0x00007fd47f0b7000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fd47ee00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fd47f0b9000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007ff95ef56000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007ff95eec3000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ff95ec00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007ff95ef58000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f04dcdf5000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f04dcc3b000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f04dca00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f04dcdf7000)
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
| local (`array`) | 264.5 (263.3–377.5) |
| global (`array_global`) | 263.0 (257.9–265.3) |

## Lectura de esta corrida

En tiempo de ejecución Copihue queda por encima de los otros tres en las ocho filas. El cociente Copihue/C más alto es `fold` (422.75×) y el más bajo es `chain` (1.09×). Esos dos extremos no dicen lo mismo: en `fold` el decimal del resultado está en el ensamblado de gcc y de rustc, y el tiempo de esos dos es el de arrancar el proceso; OCaml sí ejecuta el bucle y ahí el cociente es el de la tabla (Copihue/OCaml). En `chain` los cuatro tiempos quedan juntos: es una cadena dependiente de multiplicar, y el `push`/`pop` de Copihue no se ve casi. El hueco grande, en los programas cuyo bucle sí corre en los cuatro, está en `sort`, `array`, `fib`, `matmul` y `arith`.

En tiempo de compilación de estos archivos chicos, la mediana de Copihue es la más baja en las ocho filas. Eso mide el arranque de un compilador de una pasada más nasm y ld, no un proyecto grande.

El fichero de Copihue es el más chico de cada fila. `file` marca los binarios de Rust y de OCaml con información de depuración; la columna `text` de `size` es el código, y también ahí Copihue es el más chico. El binario de C está enlazado con libc y el de Copihue no: el tamaño no es "bytes del algoritmo".

El array local y el global de Copihue dieron medianas casi iguales en `array`. Esta corrida no muestra que el chequeo de límites sea lo que domina ese programa.

## Límites

- Esto no mide C++. El C de la tabla es `gcc -O2`.
- El tamaño del fichero mezcla el código del programa con el runtime y el tipo de enlace. No es una comparación de "bytes de algoritmo".
- El tiempo de ejecución incluye crear el proceso, tocar la pila y escribir una línea. No es un bucle interno cronometrado con `rdtsc`.
- Máquina virtual. La mediana recorta un pico, no convierte el número en una cifra de hardware físico.
- No se midió código freestanding, ni un programa grande, ni el uso de memoria.
- Copihue reserva en la pila 8 bytes por elemento aunque el elemento sea `u8` (el hueco se redondea al número de elementos). La criba local de 400000 bytes lógicos ocupa unos 3.2 MB de marco. Cabe en el `ulimit -s` de esta corrida.
- Las variables de Copihue se declaran una vez por función. Volver a hacer `let` del mismo nombre dentro de un bucle mezcla ranuras; los programas de esta suite no lo hacen.

Los datos crudos, en nanosegundos, están en `bench/results.tsv`.
