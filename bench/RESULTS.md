# Resultados

Generado por `bench/run.sh`. Las muestras se tomaron entre 2026-10-26 00:11:59 UTC y 2026-10-26 00:12:47 UTC. Este archivo se escribió el 2026-10-26 00:15:51 UTC a partir de esas muestras, en la misma máquina. `make bench` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | 2026-10-26 00:11:59 UTC — 2026-10-26 00:12:47 UTC |
| Tablas escritas | 2026-10-26 00:15:51 UTC |
| Sistema | Linux 6.12.94+ x86_64 |
| CPU | Intel(R) Xeon(R) Processor |
| Frecuencia que reporta `/proc/cpuinfo` | 2700.000 MHz |
| CPUs en línea | 4 |
| `ulimit -s` | 8192 KB |
| `getconf LONG_BIT` | 64 |
| Copihue | Copihue 0.31.1, flag `-O2` |
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
- Copihue `-O2` en hosted pliega constantes, reescribe `*`, `/` y `%` por potencias de dos, evalúa una operación de operandos simples directo en el registro del local (o en `rax`/`rbx` si hace falta un temporal), mantiene hasta cuatro locales escalares en `r12`–`r15` y, cuando ninguna llamada cae dentro del rango de uso, dos más en `r10` y `r11`, direcciona un array con `base+índice*escala` en una instrucción, y si la función abre con `if (param <op> hoja) return param` compara el registro de entrada y retorna antes de armar el frame. Una llamada directa del mismo programa, con a lo más seis argumentos que quepan en `rdi`–`r9`, los deja en esos registros; `syscall`, los builtins y un argumento que es otra llamada siguen con `push`/`pop`. Un `if` o `while` cuya condición es una comparación de escalares, o de un elemento de array, salta con las banderas y no materializa 0/1; `&&` y `||` también, y `&&` no evalúa la derecha si la izquierda ya falla. Esa carga se reutiliza en el `a[i]` o `a[i±constante] =` siguiente. Un `while (i < lim)` que arranca en una constante no negativa, solo suma uno a `i` al final, no llama y no toma direcciones, omite el chequeo de límites del array local cuando el arreglo tiene al menos `lim` elementos. El producto `s = s + a[i*n+k] * b[k*n+j]` con `k` de 0 a `n` camina las dos filas con punteros si `i` y `j` están en rango; si no, corre el bucle original. El desplazamiento `j = i - 1; while (j >= 0 && a[j] > key) { a[j+1] = a[j]; j = j - 1; } a[j+1] = key`, dentro de `while (i < N)` con `N` constante positiva, `i` desde una constante no negativa y el array `i64` de al menos `N` elementos, gira el bucle y no chequea el tope de `j`: `j` no baja de -1. `a[j] > key && j >= 0` no entra. Una función que no usa `rbp` y no empuja guarda los registros con `push`/`pop`. Un store de una constante que cabe en el elemento es un `mov` inmediato. Un llenado `a[i] = C` de un array `u8`/`i8`, con `i` desde 0 y el límite dentro del largo, es `rep stosb`. Una suma `s = s + A*i + B` con `i` de una constante a otra, paso 1, se sustituye por el total solo si cada producto y cada suma parcial cabe en i64 y los términos no cambian de signo; si puede envolver, el bucle se ejecuta. Una multiplicación de un registro por 3, 5 o 9 es un `lea`. Una racha de al menos dos `x = (x + y) & imm`, con `imm` entre 0 y 2147483647 y sin que `x` ni `y` se hayan escrito antes en la racha, emite los `add` y después los `and`. No invierte el `while` (medido, no bajó `arith`) y no desenrolla el producto interno (medido, no bajó `matmul`). Un bucle `k` probado al final, con `imul` desde memoria, también se midió y no bajó `matmul`. No borra un bucle arbitrario y no reescribe `fib`. Una copia `i = 0; while (i < lim) { dst[i] = src[i]; i = i + 1; }` de arrays `u8`/`i8`, con `lim` constante y dentro de ambos largos, es `rep movsb`. Un `rep movsb` con `rcx = 0` no copia. Un llenado `(buf[i] + i + r) & 255` desde `i = 0`, con `lim` constante, múltiplo de 16 y dentro del largo, es `paddb` (SSE2, `movdqu`). `sum = sum + a[i]` y `b[i] = (b[i] + imm) & 255` con `lim` múltiplo de 16 y `a` de `u8` es `psadbw` más `paddb`. Un store de byte `a[i] + imm` que no es ese bucle es `add byte`, o `add al` si los arrays son distintos. Un stride potencia de dos que no es 1, 2, 4 ni 8 se escala con `shl` (`sal` es el mismo opcode). `while (i >= 0) { sum = sum + g[i].f; i = g[i].h; }` sobre un array global de structs de tamaño potencia de dos, con los dos campos de 8 bytes, es un `shl`, un `lea` y dos cargas. `* 10` sigue siendo `imul`: en `parse` el `lea` de `(x+x*4)*2` y el de `v*10+(c-48)` no separaron los rangos. `rep stosb` sigue siendo solo el llenado constante de un local. El `matmul` de gcc `-O2` vectoriza el producto con `pmuludq` y no cambia el orden `i, j, k`; eso no se emite. El `fib` de gcc `-O2` parte la recurrencia en pares e impares (`sub rsp, 200`, una sola llamada); no es una limpieza de `mov` del prólogo directo, y tampoco se emite. `-O0`, `--freestanding`, `--flat` y `--boot` no usan eso.
- C usa `long` (64 bits aquí). Rust usa `i64`. OCaml usa `int` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de `rustc -O` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, `unsafe`, pragmas, `-march=native` ni `-C opt-level=3`. Los flags son los pedidos: `gcc -O2`, `rustc -O`, `ocamlopt -O3`, `copihue -O2`. `list.rs` deriva `Copy` y `Clone` solo para poder construir el array fijo.
- `fill`, `copy`, `hash` y `parse` recorren bytes. En Copihue y en C el buffer es una global (`.bss`, un byte por elemento). En Rust es un array en la pila. En OCaml es `Bytes` en el heap. `list` es un array de nodos `{val, next}` con `next` entero y centinela `-1`. OCaml guarda el par en un array plano (`2*i`, `2*i+1`) porque un record es un bloque con cabecera, no 16 bytes contiguos; la cadena de índices es la misma. No hay `malloc`.
- Esta copia de ocamlopt no trae Flambda. `-O3` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
| `fib` | fib(40) | 7.1 (7.0–7.7) | 43.3 (43.0–43.8) | 95.4 (91.2–98.0) | 66.0 (64.3–69.6) |
| `sieve` | sieve limit=400000 reps=80 | 8.0 (8.0–8.5) | 30.6 (29.7–32.5) | 101.8 (96.1–106.5) | 66.4 (66.0–68.4) |
| `matmul` | matmul N=384 | 9.0 (8.8–9.5) | 34.1 (33.4–34.8) | 114.3 (112.8–123.3) | 71.5 (70.2–74.1) |
| `arith` | arith n=100000000 | 6.9 (6.4–7.7) | 29.0 (27.4–32.5) | 106.5 (105.6–119.8) | 70.6 (69.3–71.1) |
| `chain` | chain n=80000000 | 7.4 (7.0–8.0) | 28.5 (27.9–31.6) | 111.3 (109.8–116.1) | 69.2 (67.7–72.1) |
| `fold` | fold n=500000000 | 7.0 (6.3–7.3) | 27.1 (26.2–27.9) | 103.1 (99.2–108.8) | 68.3 (67.2–69.6) |
| `array` | array n=100000 rounds=1000 | 7.2 (7.0–7.5) | 31.1 (30.2–32.3) | 112.8 (109.7–118.9) | 70.3 (68.0–72.5) |
| `sort` | sort n=28000 | 7.7 (7.2–7.8) | 32.4 (30.4–32.6) | 114.8 (111.5–121.7) | 69.9 (69.1–73.0) |
| `fill` | fill n=262144 reps=128 | 7.7 (7.0–7.9) | 33.8 (33.6–35.5) | 112.3 (107.5–115.0) | 72.3 (70.4–73.7) |
| `copy` | copy n=262144 reps=192 | 8.1 (7.6–8.3) | 34.4 (33.8–35.2) | 113.0 (107.1–117.2) | 71.6 (69.1–73.2) |
| `hash` | hash n=131072 reps=256 | 7.9 (6.8–8.4) | 32.2 (31.0–32.6) | 116.4 (109.0–125.5) | 74.1 (71.1–80.9) |
| `parse` | parse n=65536 reps=512 | 8.0 (6.9–9.2) | 31.7 (30.9–37.2) | 124.5 (109.4–127.8) | 72.7 (69.5–84.5) |
| `list` | list n=32768 reps=512 | 7.2 (7.0–7.6) | 30.5 (29.6–33.9) | 116.1 (109.8–128.8) | 72.6 (66.0–76.7) |

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
| `fib` | 9624 (385/1/0) | 15992 (2743/600/8) | 3848936 (331344/10968/304) | 1062264 (621110/136940/28200) |
| `sieve` | 10136 (672/21/0) | 16064 (1875/616/8) | 3848880 (331712/10968/304) | 1062184 (621559/136908/28200) |
| `matmul` | 10488 (1018/61/0) | 16064 (2158/616/8) | 3848880 (331928/10968/304) | 1116688 (651959/147596/28200) |
| `arith` | 9616 (435/1/0) | 15968 (1477/600/8) | 3848880 (331320/10968/304) | 1062088 (620959/136812/28200) |
| `chain` | 9616 (406/1/0) | 15968 (1461/600/8) | 3848880 (331352/10968/304) | 1062088 (620959/136812/28200) |
| `fold` | 9520 (380/1/0) | 15968 (1413/600/8) | 3848880 (331232/10968/304) | 1062072 (620935/136812/28200) |
| `array` | 9912 (588/1/0) | 16016 (1762/608/8) | 3848880 (331880/10968/304) | 1116552 (651503/147468/28200) |
| `sort` | 9968 (659/21/0) | 16016 (1778/608/8) | 3848880 (331944/10992/304) | 1116504 (651527/147436/28200) |
| `fill` | 9936 (582/1/262148) | 15992 (2144/600/262176) | 3848880 (331904/10968/304) | 1062200 (621351/136940/28200) |
| `copy` | 9872 (602/1/524292) | 16064 (2324/608/524320) | 3848880 (331808/10968/304) | 1066392 (621631/137036/28200) |
| `hash` | 9904 (494/1/131076) | 15992 (1952/600/131104) | 3848880 (331768/10968/304) | 1062168 (621271/136908/28200) |
| `parse` | 10168 (586/1/65540) | 16000 (1557/600/65568) | 3848880 (331776/10968/304) | 1062216 (621431/136940/28200) |
| `list` | 9880 (586/1/524292) | 16000 (1532/600/524320) | 3848880 (331592/10992/304) | 1116504 (651407/147436/28200) |

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `fib` | 102334155 | 393.4 (392.9–397.3) | 131.0 (130.6–131.5) | 193.9 (193.2–194.3) | 366.0 (365.3–368.4) | 3.00 | 2.03 | 1.08 |
| `sieve` | 6458901531 | 72.4 (72.0–72.7) | 70.5 (70.1–72.8) | 43.4 (43.1–43.8) | 128.1 (127.8–129.2) | 1.03 | 1.67 | 0.57 |
| `matmul` | 693633024 | 46.7 (46.2–47.3) | 31.9 (31.6–32.3) | 50.5 (50.1–50.7) | 78.4 (78.1–80.3) | 1.47 | 0.93 | 0.60 |
| `arith` | 3218085318 | 63.5 (62.8–64.0) | 59.6 (59.1–60.1) | 55.9 (55.7–56.6) | 66.9 (66.5–69.4) | 1.07 | 1.14 | 0.95 |
| `chain` | 215544461 | 166.9 (166.5–167.8) | 187.7 (187.3–189.5) | 182.2 (181.9–182.9) | 187.9 (187.6–188.5) | 0.89 | 0.92 | 0.89 |
| `fold` | 374999999750000000 | 1.7 (1.3–2.1) | 1.5 (1.3–2.0) | 1.5 (1.4–2.0) | 249.5 (234.8–256.0) | 1.20 | 1.19 | 0.01 |
| `array` | 2557452942256 | 45.7 (45.5–46.6) | 44.7 (42.5–51.4) | 33.7 (33.2–34.2) | 59.4 (58.9–59.7) | 1.02 | 1.35 | 0.77 |
| `sort` | 1070217027633 | 52.6 (52.3–53.0) | 73.5 (73.2–74.0) | 53.4 (53.0–53.9) | 154.3 (153.5–154.5) | 0.72 | 0.99 | 0.34 |
| `fill` | 4286578688 | 17.5 (16.7–17.6) | 11.5 (11.3–11.7) | 12.6 (12.4–13.1) | 92.0 (91.4–92.4) | 1.52 | 1.39 | 0.19 |
| `copy` | 6417285120 | 4.7 (4.4–5.2) | 10.0 (9.7–10.6) | 17.4 (17.2–19.5) | 164.1 (163.6–164.7) | 0.47 | 0.27 | 0.03 |
| `hash` | 177127408896 | 45.1 (45.0–45.4) | 46.6 (46.1–48.0) | 39.0 (38.8–40.0) | 46.9 (46.2–47.2) | 0.97 | 1.16 | 0.96 |
| `parse` | 41421435022848 | 22.3 (21.9–23.1) | 14.0 (13.4–14.3) | 14.6 (13.5–15.9) | 34.2 (33.2–34.7) | 1.60 | 1.53 | 0.65 |
| `list` | 274869518336 | 41.0 (40.7–41.4) | 33.5 (33.2–33.7) | 42.2 (42.0–42.3) | 31.0 (30.9–31.4) | 1.22 | 0.97 | 1.32 |

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
	linux-vdso.so.1 (0x00007f9e432ac000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9e43000000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f9e432ae000)
```

### fib rs

```
/workspace/bench/out/bin/fib_rs: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=60a688d408ce3b39d85f0efd6c7cd70dc5e1ef56, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007fd884c0a000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007fd884b77000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fd884800000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fd884c0c000)
```

### fib ml

```
/workspace/bench/out/bin/fib_ml: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=3696434e115e14f52c1f11eac0f27956aacd7f73, for GNU/Linux 3.2.0, with debug_info, not stripped
	linux-vdso.so.1 (0x00007f0a30aea000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f0a30930000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f0a30600000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f0a30aec000)
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

## `fill`, `copy`, `hash`, `parse` y `list`: el decimal en el `.s`

La misma búsqueda, con la salida de cada programa. `sí` quiere decir que ese decimal está en el ensamblado y el tiempo no es el del bucle.

| Programa | gcc | rustc | ocamlopt | copihue |
|---|---|---|---|---|
| `fill` | no | no | no | no |
| `copy` | no | no | no | no |
| `hash` | no | no | no | no |
| `parse` | no | no | no | no |
| `list` | no | no | no | no |

## Array local y array global, solo Copihue

`bench/src/array_global.lap` es el mismo algoritmo que `array`, con el arreglo en una global. Una carga de un array local emite un chequeo de límites, salvo el `while (i < lim)` de la metodología: el bucle interno de `array` y su suma final son de esa forma, así que no lo emiten. La global no lo emite. La salida coincidió con `array`.

| Variante | Mediana (mín–máx) ms |
|---|---:|
| local (`array`) | 45.7 (45.5–46.6) |
| global (`array_global`) | 45.3 (45.1–45.7) |

## Lectura de esta corrida

El cociente Copihue/C más alto es `fib` (3.00×) y el más bajo es `copy` (0.47×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En `fold` la tabla del decimal dice gcc sí, rustc sí, Copihue sí y OCaml no. Quien lo tiene no ejecutó las 500000000 vueltas, y ese tiempo es el de arrancar el proceso. Quien no lo tiene sí ejecuta el bucle; el cociente Copihue/OCaml es el de la tabla. `chain` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone.

Contra C, el mínimo de Copihue queda por encima del máximo en: `fib` `matmul` `arith` `fill` `parse` `list`. El máximo de Copihue queda por debajo del mínimo en: `chain` `sort` `copy` `hash`. Los rangos se solapan en: `sieve` `fold` `array`.
Contra Rust, el mínimo de Copihue queda por encima del máximo en: `fib` `sieve` `arith` `array` `fill` `hash` `parse`. El máximo de Copihue queda por debajo del mínimo en: `matmul` `chain` `copy` `list`. Los rangos se solapan en: `fold` `sort`.
Contra OCaml, el mínimo de Copihue queda por encima del máximo en: `fib` `list`. El máximo de Copihue queda por debajo del mínimo en: `sieve` `matmul` `arith` `chain` `fold` `array` `sort` `fill` `copy` `hash` `parse`. Los rangos se solapan en: ninguna.

En tiempo de compilación de estos archivos chicos, la mediana de Copihue es la más baja en las 13 filas. Eso mide el arranque de un compilador de una pasada más nasm y ld, no un proyecto grande.

El fichero de Copihue es el más chico de cada fila. `file` marca los binarios de Rust y de OCaml con información de depuración; la columna `text` de `size` es el código, y también ahí Copihue es el más chico. El binario de C está enlazado con libc y el de Copihue no: el tamaño no es "bytes del algoritmo".

La mediana del array local y la del global están en la tabla de arriba. El hueco que quede entre las dos está medido ahí; el perfil de más abajo cuenta las instrucciones del bucle interno.

## Límites

- Esto no mide C++. El C de la tabla es `gcc -O2`.
- El tamaño del fichero mezcla el código del programa con el runtime y el tipo de enlace. No es una comparación de "bytes de algoritmo".
- El tiempo de ejecución incluye crear el proceso, tocar la pila y escribir una línea. No es un bucle interno cronometrado con `rdtsc`.
- Máquina virtual. La mediana recorta un pico, no convierte el número en una cifra de hardware físico.
- No se midió código freestanding, ni un programa grande, ni el uso de memoria.
- Copihue reserva en la pila 8 bytes por elemento aunque el elemento sea `u8` (el hueco se redondea al número de elementos). La criba local de 400000 bytes lógicos ocupa unos 3.2 MB de marco. Cabe en el `ulimit -s` de esta corrida. `fill`, `copy`, `hash` y `parse` no usan ese marco: el buffer es global y el elemento ocupa un byte. `list` reserva 32768 nodos de 16 bytes en `.bss`.
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

## 0.29.3 → 0.30.1

`main` tomó 0.29.4 para restaurar `ebx` en la aritmética de `bits 16` y `bits 32`, y 0.30.0 para la aritmética al ancho del tipo. Esta ronda es 0.30.1 y no toca esos caminos. La ventana de esta página es 2026-10-25 23:29:22 UTC — 2026-10-25 23:30:09 UTC. El binario imprime 0.30.1. La referencia es la ventana 22:33:23–22:34:10 UTC (el binario imprimía 0.29.2; el `-O2` hosted de 0.29.3 coincide con él salvo la versión). Mediana de Copihue en milisegundos (mínimo–máximo). El cociente es la referencia / esta medición.

| Programa | 22:33 (0.29.3) | 0.30.1 | referencia / 0.30.1 |
|---|---:|---:|---:|
| `copy` | 47.8 (47.5–49.0) | 42.0 (41.5–42.2) | 1.14 |
| `fill` | 32.4 (32.1–32.7) | 17.2 (16.6–17.7) | 1.88 |
| `list` | 53.4 (53.0–54.2) | 40.4 (40.3–41.0) | 1.32 |
| `parse` | 22.3 (21.9–22.5) | 22.5 (22.2–24.1) | 0.99 |

`copy`, `fill` y `list` bajan, y los rangos no se solapan. `parse` se solapa. Su ensamblado `-O2` es el de 0.29.3, salvo la línea de versión, y `text` sigue en 586. `fib`, `sieve`, `matmul`, `arith`, `chain`, `fold`, `array`, `sort`, `hash`, `array_global`, `fill` y `list` coinciden con el ensamblado de un binario que imprimía 0.29.5 (ventana 23:19:30–23:20:17 UTC), salvo la línea de versión. `text` de los diez que no se reescriben no cambia (`fib` 385, `sieve` 672, `matmul` 1018, `arith` 435, `chain` 406, `fold` 380, `array` 588, `sort` 659, `hash` 494; `array_global` sigue en 549). `fill` pasa de 493 a 582 y `list` de 627 a 586.

`copy` no coincide con ese binario de 0.29.5. El primer bucle es el mismo `rep movsb`. El segundo, `src[i] = (src[i] + 1) & 255`, gana dos `movzx rax, al` porque 0.30.0 trunca la suma `u8` y la máscara `u8`. En 23:19, sin esos dos `movzx`, `copy` fue 37.7 ms (37.5–38.4). Esta ventana, con ellos, es 42.0 ms (41.5–42.2). `text` pasa de 534 a 557 (549 en el binario de 23:19). El rango sigue por debajo de 47.5–49.0.

La copia reconocible es `rep movsb` (`push rsi` / `push rdi` / `pushfq` / `cld` / dos `lea` / `mov rcx, 262144` / `rep movsb` / los tres `pop`) y luego `mov` de `i` al límite. El bucle que sigue, `sum = sum + dst[i]` y `src[i] = (src[i] + 1) & 255`, queda escalar, con esos dos `movzx`. Por eso `copy` sigue en 4.35× C (41.5–42.2 contra 9.2–10.2), por encima de Rust (16.6–17.4) y por debajo de OCaml (162.6–163.5). gcc sigue con `call memcpy` en la copia.

El llenado `(buf[i] + i + r) & 255` es ocho instrucciones: `movdqu` / `paddb` índice / `paddb` de `r` / `movdqu` / `paddb` de 16 / `add rax, 16` / `sub rcx` / `jnz`. El vector de índice arranca en los bytes 0..15. `movdqu` no pide alineación a 16. No es `rep stosb`: el byte no es constante. `fill` queda en 1.52× C (16.6–17.7 contra 10.9–11.7), por encima de Rust (11.9–13.6) y por debajo de OCaml (91.0–91.8). La suma del buffer sigue escalar. Un largo que no es múltiplo de 16 no entra.

El salto de `list` es `mov rax, i` / `shl rax, 4` / `lea rax, [nodes+rax]` / `add sum, [rax]` / `mov i, [rax+8]` / `cmp i, 0` / `jge`. Siete instrucciones. Antes eran dos `imul rax, 16` y un `push`/`pop` del acumulador. `sal` y `shl` son el mismo opcode; el emisor escribe `shl`. Un array local no entra, para no saltarse el chequeo de límites. Un struct de 24 bytes sigue con `imul`. En esta ventana `list` queda por encima de C (40.3–41.0 contra 32.8–33.7) y de OCaml (30.3–31.1). Queda por debajo de Rust (41.5–42.3); los rangos no se solapan. El cociente de medianas contra C es 1.22 y contra Rust 0.97. Esto es esta fila y estas corridas.

`* 10` sigue siendo `imul`. Se midió `lea` de `(x+x*4)*2`, y el `lea` fusionado de `v * 10 + (c - 48)`, antes de la ventana 23:19 (un calentamiento y siete corridas, `taskset -c 0`). `parse` quedó en 22.6 ms (22.2–22.9) contra 22.3 ms (21.9–22.5). Los rangos se solapan, y esa forma no está. En esta ventana, con `imul`, `parse` es 22.5 ms (22.2–24.1), 1.60× C (13.5–14.2), por encima de C y de Rust (13.7–15.0) y por debajo de OCaml (33.5–34.9).

`hash` se solapa con C (44.6–45.3 contra 45.1–47.2). El cociente de medianas es 0.99. No queda por debajo de C. El ensamblado no cambió. `sort` se solapa con Rust (52.6–53.0 contra 52.6–53.7); el cociente de medianas es 0.99. `fold` se solapa con C (1.0–1.8 contra 1.1–1.8) y con Rust (1.2–1.8). `chain` queda por debajo de los tres (166.0–166.7 contra C 186.8–187.3, Rust 181.5–182.4 y OCaml 187.2–187.8). El binario es el de 0.25.1, y esto es esta fila y estas corridas. `fib` sigue en 3.01× C (392.7–394.0 contra 130.5–131.2). El prólogo de `n >= 2` sigue en 16 instrucciones. gcc `-O2` de `matmul` sigue con `pmuludq`; eso no se emite.

`-O0`, `--freestanding`, `--flat` y `--boot` no entran en estas tres reescrituras. `bits 16` y `bits 32` tampoco: siguen en el arreglo de `ebx` de 0.29.4.

## 0.31.0 → 0.31.1

`main` tomó 0.31.0 para rechazar un entero más ancho en la asignación, la llamada directa y el `return`. Esta ronda es 0.31.1. La ventana de esta página es 2026-10-26 00:11:59 UTC — 2026-10-26 00:12:47 UTC. El binario imprime 0.31.1. La referencia de `copy` es la ventana 23:29:22–23:30:09 UTC (el binario imprimía 0.30.1): 42.0 ms (41.5–42.2). Mediana de Copihue en milisegundos (mínimo–máximo).

| Programa | 23:29 (0.30.1) | 0.31.1 | 23:29 / 0.31.1 |
|---|---:|---:|---:|
| `copy` | 42.0 (41.5–42.2) | 4.7 (4.4–5.2) | 8.94 |

`copy` baja, y los rangos no se solapan. `text` pasa de 557 a 602. El primer bucle sigue siendo `rep movsb`. El segundo, `sum = sum + dst[i]` y `src[i] = (src[i] + 1) & 255`, es `psadbw` de los bytes viejos y `paddb` del sumando, dieciséis bytes por vuelta (`movdqu`). `a` es `u8`. Un `i8` no entra. `(e) as u8` se pela y es el mismo byte. Un store suelto `a[i] = (a[i] + imm) & 255`, `a[i] = a[i] + imm` o `dst[i] = src[i] + imm` es `add byte` si es el mismo array, y `movzx` / `add al` / `mov byte` si son dos. Esos dos `movzx` del camino escalar no se emiten en ese store. En esta ventana `copy` queda por debajo de C (4.4–5.2 contra 9.7–10.6), de Rust (17.2–19.5) y de OCaml (163.6–164.7). El cociente contra C es 0.47. Esto es esta fila y estas corridas. gcc sigue con `call memcpy` en la copia y con su propio `paddb` en el incremento.

`fill` queda en 17.5 ms (16.7–17.6) contra 17.2 (16.6–17.7). Los rangos se solapan. Sigue en 1.52× C (11.3–11.7), por encima de Rust (12.4–13.1) y por debajo de OCaml (91.4–92.4). `text` sigue en 582. `list` queda en 41.0 ms (40.7–41.4) contra 40.4 (40.3–41.0). Se solapan. Sigue por encima de C (33.2–33.7) y de OCaml (30.9–31.4). Queda por debajo de Rust (42.0–42.3); los rangos no se solapan. El cociente de medianas contra C es 1.22 y contra Rust 0.97. Esto es esta fila y estas corridas. `text` sigue en 586.

`parse` queda en 22.3 ms (21.9–23.1) contra 22.5 (22.2–24.1). Se solapan. Sigue en 1.60× C (13.4–14.3), por encima de Rust (13.5–15.9) y por debajo de OCaml (33.2–34.7). `text` sigue en 586. `* 10` sigue siendo `imul`. Antes de esta ventana, con la misma `n` y las mismas `reps`, un calentamiento y siete corridas, `taskset -c 0`: el bucle real 22 ms (22–22); carga, comparación y `v = v * 10 + 1` 22 ms (22–24); carga, comparación y `v = v + 1` 22 ms (22–23); nueve `imul` dependientes y un reinicio, sin carga de byte, 10 ms (10–14). Quitar el `imul` no mueve el bucle por byte. La carga y la comparación son el costo que se ve, y no hay una reescritura de ese bucle medida más rápida en el bench oficial.

`fib` queda en 393.4 ms (392.9–397.3), 3.00× C (130.6–131.5), por encima de Rust (193.2–194.3) y de OCaml (365.3–368.4). El prólogo de `n >= 2` sigue en 16 instrucciones y el conteo de llamadas no cambia. Se midió reusar `r12` para el segundo llamado (sigue habiendo dos `call`, sin el segundo callee-saved ni el pad de alineación): 0.31.0 fue 393, 393, 393, 393, 393, 393, 394 (mediana 393) y esa forma 393, 394, 394, 394, 394, 396, 397 (mediana 394). Se solapa con 0.31.0 y con 392.7–394.0. No está. gcc `-O2` parte pares e impares y hace una sola llamada; eso cambia el conteo y no se emite. `text` de `fib` sigue en 385.

`hash`, `sort`, `fold` y `chain` no se tocaron. El ensamblado `-O2` de `sieve`, `matmul`, `arith`, `chain`, `fold`, `array`, `sort`, `hash`, `array_global`, `fill`, `list` y `parse` coincide con 0.31.0 salvo la línea de versión. Los fuentes del banco ganaron `as u8` en el store de preparación; los bytes son los mismos. En esta ventana `hash` queda por debajo de C (45.0–45.4 contra 46.1–48.0). El código es el de antes; en 23:29 los rangos se solapaban (44.6–45.3 contra 45.1–47.2). No es una mejora de esta ronda. Sigue por encima de Rust (38.8–40.0). `sort` se solapa con Rust (52.3–53.0 contra 53.0–53.9); el cociente de medianas es 0.99. Queda por debajo de C (73.2–74.0) y de OCaml (153.5–154.5). `fold` se solapa con C (1.3–2.1 contra 1.3–2.0) y con Rust (1.4–2.0). `chain` queda por debajo de los tres (166.5–167.8 contra C 187.3–189.5, Rust 181.9–182.9 y OCaml 187.6–188.5). El binario es el de 0.25.1, y esto es esta fila y estas corridas. `sieve` se solapa con C (72.0–72.7 contra 70.1–72.8).

`-O0`, `--freestanding`, `--flat` y `--boot` no entran en `psadbw` ni en `add byte`. `bits 16` y `bits 32` tampoco.
