# Back end nativo: codificador `x64.c` (corte 1, segunda implementación)

> Nota: otra sesión escribió en paralelo, en la misma rama, un codificador distinto (`x64enc*.c`) y su documento [DESIGN_NATIVE.md](DESIGN_NATIVE.md) (commit `4043072`). Este archivo documenta el codificador `x64.c`, que llegó a la misma tarea por separado. Ambos leen el mismo corpus (181 987 instrucciones, 78 mnemónicos) y los conteos coinciden. La diferencia de cobertura (87,64 % allá, 100 % acá) viene de que `x64.c` también codifica las instrucciones con un símbolo (relocación) y los saltos a etiqueta en forma `rel32`. Conviene dejar uno solo; ver NOTAS-SESION.txt.

Meta de largo plazo: que Copihue no dependa de C ni de `nasm`/`ld`. Este documento es el paso 1: que el compilador escriba por sí mismo el ELF x86-64 del modo hosted. Hoy el compilador emite texto NASM, y `nasm -f elf64` más `ld` hacen el resto (`main()` en `copihue.c`).

Este corte hace tres cosas: mide qué emite el generador (levantamiento), propone el diseño del codificador directo y entrega la primera porción: un codificador de instrucciones, comprobado byte a byte contra nasm. **No** está conectado a `copihue.c` y no cambia ningún comportamiento: sin opción nueva, el compilador sigue usando nasm y ld. Etiquetas, relajación de saltos, secciones y la cabecera ELF están diseñadas aquí y no implementadas.

## 1. Levantamiento

### Método

`tools/native/corpus.sh` compila con `-S`, a `-O0` y a `-O2`, los 233 programas que `tests/run_tests.sh` ejecuta en modo hosted (`tests/*.lap`, `tests/*/*.lap` salvo `errors`, `freestanding`, `flat`, `boot`, `usermode`, `ctx`; `examples/**`). Salen 466 archivos `.asm` y ningún fallo de compilación. `tools/native/asminv.c` clasifica cada línea con el mismo analizador que usa el codificador (`compiler/native/x64.c`, `x64_line.c`). Se reproduce con:

```
make && make native-inventory                       # resumen
bash tools/native/inventory.sh --list-uncovered --top 45   # tablas completas
```

Los conteos son de líneas estáticas del texto generado, sumadas sobre los 466 archivos. No son frecuencias de ejecución. Los programas de red (`simple_api_server` y otros que `run_tests.sh` salta) también entran, porque solo se compilan.

### Líneas

| | `-O0` | `-O2` | Suma |
|---|---:|---:|---:|
| Líneas | 112 994 | 98 251 | 211 245 |
| Instrucciones | 98 064 | 83 923 | **181 987** |
| Etiquetas (`name:`) | 10 321 | 9 815 | 20 136 |
| Directivas | 4 130 | 4 100 | 8 230 |
| En blanco / comentario | 3 146 | 3 043 | 6 189 |

Todo el código está en una sola `section .text`; los datos en `.data` y `.bss`. Cada archivo empieza con `global _start` y define `_start`, que llama a `main` y hace `exit` con `syscall`. Las etiquetas locales (`.loop`, `.positive`) son 20 136 menos las de función; no hay macros ni `%define`.

### Directivas (466 archivos)

| Directiva | Líneas |
|---|---:|
| `db` | 4 737 |
| `equ` (`__frame_f equ N`, tamaño del marco de cada función) | 1 382 |
| `global`, `section .data`, `section .text` | 466 cada una |
| `dq` | 328 |
| `resb` | 186 |
| `section .bss` | 104 |
| `align N` / `align N, db 0` / `align N, resb 1` | 2+1 / 10 / 26 |
| `resq`, `resd`, `resw`, `dd` | 30, 18, 4, 4 |

No aparecen `extern`, `times`, `%define` ni `bits` en el modo hosted. Los datos son bytes (cadenas como `db 72, 111, ...`), `dq` con números o con la dirección de una etiqueta (`dq str_0`), y reservas en `.bss`.

### Mnemónicos: 78 distintos

Los 25 primeros cubren el 98,4 % de las instrucciones; los 40 primeros el 99,8 %.

| Mnemónico | Líneas | Acum. | Mnemónico | Líneas | Acum. |
|---|---:|---:|---|---:|---:|
| `mov` | 87 596 | 48,13 % | `jz` | 1 718 | 93,31 % |
| `push` | 14 451 | 56,07 % | `movzx` | 1 419 | 94,09 % |
| `syscall` | 10 346 | 61,76 % | `imul` | 1 319 | 94,82 % |
| `pop` | 8 288 | 66,31 % | `jge` | 1 134 | 95,44 % |
| `add` | 8 268 | 70,86 % | `jne` | 1 068 | 96,03 % |
| `ret` | 5 472 | 73,86 % | `dec` | 932 | 96,54 % |
| `leave` | 5 156 | 76,70 % | `jnz` | 767 | 96,96 % |
| `jmp` | 4 316 | 79,07 % | `neg` | 618 | 97,30 % |
| `call` | 4 230 | 81,39 % | `jl` | 571 | 97,62 % |
| `cmp` | 3 889 | 83,53 % | `movsxd` | 554 | 97,92 % |
| `xor` | 3 868 | 85,65 % | `shl` | 489 | 98,19 % |
| `lea` | 3 764 | 87,72 % | `div`, `jns` | 466 | 98,70 % |
| `test` | 2 906 | 89,32 % | `js` | 426 | 98,94 % |
| `inc` | 2 818 | 90,87 % | `je` | 280 | 99,09 % |
| `sub` | 2 735 | 92,37 % | | | |

La cola (menos de 0,3 % entre todos): `and` 212, `cqo` 191, `idiv` 191, `sar` 138, `jle` 123, `jg` 103, `shr` 96, `jae` 76, `hlt` 64, `cld` 57, `ud2` 46, `lock xadd` 44, `rep movsb` 30, `pause` 28, `ja` 24, `jb` 22, `rep stosq` 22, `lock add` 20, `movdqu` 13, `popfq` 13, `pushfq` 13, `sete` 12, `movsx` 10, `lock cmpxchg` 8, `mfence` 8, `paddb` 8, `setz` 8, `setg` 6, `rep stosb` 5, `clc`, `cpuid`, `jnc`, `movd`, `movq`, `not`, `pshufd`, `punpcklbw`, `punpcklwd`, `pxor`, `setc`, `setl`, `stc`, `xchg` (4 cada una), `movdqa`, `or`, `paddq`, `psadbw`, `psrldq` (2 cada una).

### Formas (mnemónico + clase de operandos): 135 distintas

En la tabla, `r64` es un registro de 64 bits y `r8` uno de 8 bits (no el registro `r8`); `m` es memoria sin tamaño escrito, `m8`/`m32` con tamaño; `sym` es un símbolo (etiqueta o dato); `imm` es una constante.

| Forma | Líneas | % | Acum. |
|---|---:|---:|---:|
| `mov r64,imm` | 37 522 | 20,62 | 20,62 |
| `mov r64,r64` | 24 580 | 13,51 | 34,12 |
| `push r64` | 11 107 | 6,10 | 40,23 |
| `syscall` | 10 346 | 5,69 | 45,91 |
| `mov r64,m` | 9 695 | 5,33 | 51,24 |
| `pop r64` | 8 288 | 4,55 | 55,79 |
| `mov m,r64` | 5 669 | 3,12 | 58,91 |
| `ret` | 5 472 | 3,01 | 61,92 |
| `leave` | 5 156 | 2,83 | 64,75 |
| `add r64,imm` | 4 709 | 2,59 | 67,34 |
| `mov r64,sym` | 4 709 | 2,59 | 69,92 |
| `jmp sym` | 4 308 | 2,37 | 72,29 |
| `call sym` | 4 158 | 2,28 | 74,58 |
| `lea r64,m` | 3 764 | 2,07 | 76,64 |
| `xor r64,r64` | 3 619 | 1,99 | 78,63 |
| `push imm` | 3 344 | 1,84 | 80,47 |
| `add r64,r64` | 3 035 | 1,67 | 82,14 |
| `inc r64` | 2 796 | 1,54 | 83,67 |
| `sub r64,imm` | 2 002 | 1,10 | 84,77 |
| `test r64,r64` | 1 910 | 1,05 | 85,82 |
| `cmp r64,r64` | 1 743 | 0,96 | 86,78 |
| `jz sym` | 1 718 | 0,94 | 87,73 |
| `mov r8,m` | 1 426 | 0,78 | 88,51 |
| `jge sym` | 1 134 | 0,62 | 89,13 |
| `jne sym` | 1 068 | 0,59 | 89,72 |

`mov r64,imm` incluye cada `mov rax, 5`, `mov rdi, 1`. `mov r64,sym` es `mov rax, str_0` (dirección de un dato). Las 135 formas completas con su cobertura salen de `asminv --top 135`.

Saltos y llamadas a un símbolo (`jmp`, `jcc`, `call` con etiqueta): 15 248 líneas, el 8,38 % de las instrucciones. Su destino es una etiqueta de la misma sección en todos los casos menos `call` a funciones (también de la misma sección: todo el programa es un solo objeto).

### Operandos de memoria: 25 603

| Forma | Líneas | % |
|---|---:|---:|
| `[rbp+disp8]` (variables locales) | 14 658 | 57,25 |
| `[reg]` | 4 526 | 17,68 |
| `[sym]` (dirección absoluta) | 2 084 | 8,14 |
| `[rbp+disp32]` (`[rbp - __frame_f + N]`, ya resuelto por `equ`) | 1 654 | 6,46 |
| `[reg+disp8]` | 639 | 2,50 |
| `[rbp+idx+disp8]` | 480 | 1,87 |
| `[reg+idx]` | 466 | 1,82 |
| `[rsp/r12+disp8]` | 255 | 1,00 |
| `[sym+reg]` | 333 | 1,30 |
| resto (21 formas) | | 2,0 |

El desplazamiento simbólico `[rbp - __frame_f + N]` se resuelve en el ensamblador con la constante `equ`; el codificador recibe un entero. `[sym]` y `[sym+reg*N]` son direcciones absolutas de 32 bits con signo (`R_X86_64_32S`), no relativas a RIP. Hay 4 usos de `[rel .Lnn]` (constantes de `movdqu` en `-O2`).

### Registros

Aparecen `rax`..`r15` de 64 bits, las partes de 32 (`eax`, `edi`, `esi`, `ecx`, `edx`), `ax`, `al`, `bl`, `cl`, `dl` y `xmm0`..`xmm3`. No aparecen `ah`..`bh`, registros de control, de segmento ni de x87.

### Lo que este levantamiento no cubre

Es el modo hosted. `--freestanding`, `--flat` y `--boot` agregan `bits 16`/`bits 32`, `org`, instrucciones privilegiadas y secciones con nombre; no están en este conteo y son otro corte.

## 2. Diseño

### 2.1 Codificación de una instrucción

Una instrucción es: prefijos de grupo 1 (`lock`, `rep`), prefijo de tamaño `0x66` o prefijo obligatorio SSE, `REX`, uno a tres bytes de opcode, `ModRM`, `SIB`, desplazamiento (1 o 4 bytes) e inmediato (1, 2, 4 u 8 bytes). `x64.c` construye cada instrucción con una sola estructura `Spec` y una sola función `emit`:

- `REX.W` si el tamaño es 8; `REX.R`, `REX.X`, `REX.B` por los bits altos de `reg`, índice y base/`rm`; un `REX` vacío (`0x40`) si hay `spl`, `bpl`, `sil` o `dil`; error si se mezcla `ah`..`bh` con `REX`.
- `ModRM`: `mod=11` registro; si no, base sin desplazamiento (`mod=00`, salvo base `rbp`/`r13`, que exige `disp8=0`), `disp8` (`mod=01`) si cabe en un byte con signo, `disp32` (`mod=10`) en otro caso. Base `rsp`/`r12` o índice fuerzan `SIB`. Sin base: `mod=00`, `rm=100`, `SIB` con base `101` y `disp32`. `[rsp+...]` no puede ser índice.
- Tamaño por la clase del operando (registro) o por la palabra `byte|word|dword|qword` (memoria sin registro).
- Cada familia (ALU, `mov`, `test`, desplazamientos, grupo `F6/F7`, `imul`, `push`/`pop`, saltos, `setcc`, `cmovcc`, `movzx`/`movsx`/`movsxd`, `xchg`/`xadd`/`cmpxchg`, SSE2 entero) es una rama corta que elige opcode e inmediato y llama a `emit`.

La parte difícil no es la tabla de opcodes sino **elegir la misma forma que nasm**, porque la equivalencia byte a byte es el criterio de aceptación. Lo que la comparación con nasm obligó a copiar:

- `mov r64, imm`: nasm usa `B8+r imm32` (sin `REX.W`, rellena con ceros) si el valor está en `[0, 2^32)`; `REX.W C7 /0 imm32` si está en `[-2^31, 0)`; `REX.W B8+r imm64` si no cabe. `mov rax, 5` mide 5 bytes, no 7.
- `mov r64, sym`: `REX.W B8+r imm64` con relocación `R_X86_64_64` (10 bytes). `mov eax, sym` usa `R_X86_64_32`.
- ALU con acumulador: `add rax, 1000` usa `05 imm32` (6 bytes) y no `81 /0` (7 bytes); con `imm8` usa `83 /0 ib`. `test` igual con `A8`/`A9`.
- `movsxd rax, eax` se escribe `cdqe` (`48 98`).
- `[idx*2]` sin base se escribe `[idx+idx]`.
- `xchg` con el acumulador usa `90+r`; si no, el primer operando va en `modrm.reg`.
- `push imm`: `6A ib` si cabe en 8 bits con signo, si no `68 imm32`.
- `call sym` y `jmp sym` hacia un símbolo externo: forma `rel32` con relocación `R_X86_64_PLT32` en nasm; ver la sección de pruebas para lo que mide `PC32`/`PLT32`.

### 2.2 Etiquetas y saltos con relajación

El generador usa etiquetas globales (`main`, `str_0`) y locales con punto (`.loop`) dentro de una sola sección `.text`. Propuesta:

1. **Primera pasada**: leer la lista de ítems (instrucción, etiqueta, directiva de datos, alineación). Cada instrucción se codifica con `x64_encode`; un salto o llamada a etiqueta se codifica provisoriamente en la forma corta (`jmp`/`jcc` 2 bytes) y guarda el nombre del destino.
2. **Direcciones**: recorrer los ítems sumando tamaños; una etiqueta toma la dirección acumulada; `align N` rellena (con `0x90` en `.text`; `0` en datos).
3. **Relajación**: para cada salto corto cuyo destino esté fuera de `[-128, 127]`, pasar a la forma larga (`jmp` 5 bytes, `jcc` 6). Cada cambio mueve todo lo que sigue, así que se repite hasta que ningún salto cambie. Los saltos solo crecen, nunca se acortan de nuevo, por lo que el proceso termina en como máximo (número de saltos) vueltas. `call` siempre es `rel32` (5 bytes). Esto produce el mismo resultado que nasm en la mayoría de los casos; la comparación byte a byte del programa completo es el criterio, y cualquier diferencia de tamaño de salto se documenta antes de aceptarla. `x64_encode_branch` ya produce las dos formas con desplazamiento conocido (454 casos comparados con nasm, ver abajo).
4. **Última pasada**: parchear cada `rel` con `destino - fin_de_instrucción`.

Coste: dos recorridos lineales más las vueltas de relajación; los programas del corpus tienen hasta unas decenas de miles de instrucciones.

### 2.3 Relocaciones

Hay dos caminos y conviene separarlos:

- **Ejecutable (sin `-c`)**: un solo objeto y direcciones finales conocidas. No se necesita ninguna tabla de relocaciones: cada `R_X86_64_32S`/`32`/`64`/`PC32` se resuelve al terminar el diseño de secciones con la dirección real del símbolo. `XReloc` (offset, tipo, símbolo, addendo) ya lo describe; el escritor del ELF lo aplica sobre el buffer.
- **Objeto (`-c`)**: se escribe un `ELF64 REL` con `.symtab`, `.strtab`, `.rela.text` y `.rela.data`, con los mismos cuatro tipos. Hoy `-c` pasa por nasm; moverlo al codificador es el último paso del plan, porque ahí sí hay que reproducir la tabla de símbolos de nasm (globales, locales, tamaño).

Tipos que emite el generador hosted: `R_X86_64_32S` (memoria `[sym]`, `[sym+reg*N]`, `mov qword [..], sym`, `push sym`, ALU con `sym`), `R_X86_64_64` (`mov r64, sym`, `dq sym`), `R_X86_64_PC32`/`PLT32` (`call`/`jmp`/`jcc` y `[rel x]`) y `R_X86_64_32` (`mov r32, sym`).

### 2.4 Secciones

`.text`, `.data` y `.bss` en ese orden de dirección. `.data` contiene `db`/`dq`/`dd` con valores literales o con la dirección de una etiqueta (`dq str_0`, a relocar); `.bss` solo reservas (`resb`/`resw`/`resd`/`resq`, con `align N, resb 1` para alinear). Datos con `equ` no ocupan espacio: son constantes del ensamblador, y el codificador debe recibirlas ya sustituidas (lo hace `x64_set_consts`).

### 2.5 Cabecera ELF

Un ejecutable estático no necesita cabeceras de sección. Basta con:

- `Elf64_Ehdr` (64 bytes): `ET_EXEC`, `EM_X86_64`, `e_entry = _start`, `e_phnum = 2`.
- `PT_LOAD` 1: `R+X`, desde el offset 0 (cubre las cabeceras y `.text`), dirección `0x400000` (la que usa `ld` por omisión).
- `PT_LOAD` 2: `R+W`, `.data` y `.bss`, con `p_memsz > p_filesz` para el `.bss`, alineado a página (`0x1000`).
- Sin `PT_INTERP` ni `PT_DYNAMIC`. Sin `PT_GNU_STACK`: el kernel de Linux trata la pila como no ejecutable por omisión en x86-64 si falta; conviene emitirlo explícito para igualar a `ld`.

Ya existe un precedente en el propio compilador: `emit_flat_elf` escribe un ELF con secciones con nombre para `--flat --elf`. El modo hosted reutiliza la idea con dos segmentos.

### 2.6 Orden de implementación

| Paso | Qué | Estado |
|---|---|---|
| 1 | Analizador de líneas NASM del generador y levantamiento (`x64_line.c`, `asminv`) | hecho |
| 2 | Codificador de instrucciones sin etiquetas, con relocaciones, comparado con nasm (`x64.c`, `tests/x64`) | hecho para todo el corpus hosted |
| 3 | Ítems del programa, etiquetas, direcciones y relajación; comparar el `.text` de cada programa del corpus con el de nasm | no empezado |
| 4 | Datos: `db`/`dq`/`resb`/`align`, `.data` y `.bss` | no empezado |
| 5 | Escritor del ELF ejecutable (dos `PT_LOAD`) detrás de una opción (p. ej. `--native`) | no empezado |
| 6 | Ejecutar `make test` con `--native` en CI y comparar la salida de cada programa con la de nasm+ld | no empezado; solo se puede en Linux |
| 7 | Objeto `-c` con `.symtab` y `.rela` | no empezado |
| 8 | Hacer `--native` el valor por omisión cuando 6 pase durante varias versiones | no empezado |
| 9 | `--freestanding`/`--flat`/`--boot` (`bits 16`/`32`, instrucciones privilegiadas) | no empezado |

El generador seguirá escribiendo texto NASM mientras tanto: el codificador lee ese mismo texto. Más adelante el generador puede emitir `XInsn` directamente y el texto queda solo para `-S`.

## 3. Primera porción: qué hay y qué mide

Archivos:

- `compiler/native/x64.h`, `x64.c`: analizador de una instrucción en texto NASM a `XInsn`; `x64_encode` (bytes y a lo sumo una relocación); `x64_encode_branch` (`rel8`/`rel32` con desplazamiento conocido).
- `compiler/native/x64_line.c`: clasifica una línea (etiqueta, directiva, `equ`, instrucción).
- `tools/native/corpus.sh`, `inventory.sh`, `asminv.c`: levantamiento.
- `tests/x64/run.sh`, `test_x64.c`, `vectors.asm`: la prueba. `make native-test`.

La prueba reúne las instrucciones distintas (después de sustituir las constantes `equ` y de renombrar cada símbolo a `symN`), las codifica con `x64.c`, las ensambla todas juntas con `nasm -f elf64`, lee `.text` y `.rela.text` del objeto resultante y compara por instrucción los bytes y la relocación (offset, tipo, símbolo, addendo). Separa las instrucciones con un marcador de 8 bytes para no perder la sincronía si una difiere en tamaño. Falla (código de salida 1) si alguna instrucción que el codificador acepta difiere de nasm, o si nasm rechaza el lote. Una instrucción que el codificador declara no cubierta no falla; se cuenta.

Resultado en esta máquina (macOS arm64, nasm de Homebrew):

| Conjunto | Distintas | Ocurrencias | Cubiertas | Iguales a nasm |
|---|---:|---:|---:|---:|
| Corpus hosted `-O0` + `-O2` | 4 334 | 181 987 | 181 987 (100,00 %) | 181 987 (100,00 %) |
| Vectores a mano (`vectors.asm`, bordes de REX, `rsp`/`rbp`, inmediatos, SSE) | 412 | 412 | 408 (99,03 %) | 408 |
| Saltos con desplazamiento conocido (21 saltos × formas, `call`) | 454 | 454 | 454 | 454 |

Lectura honesta del 100 %:

- Es el porcentaje de **líneas de instrucción** del corpus hosted para las que el codificador produce los mismos bytes y la misma relocación que nasm. No es el porcentaje de un programa terminado: las 8 230 directivas y 20 136 etiquetas no se emiten aún (ver pasos 3 y 4), y no hay cabecera ELF.
- Los saltos a etiqueta se codifican en forma `rel32` con una relocación contra el símbolo, que es lo que hace nasm con un símbolo externo. Eso no es lo que hará el compilador con las etiquetas de la misma sección (ahí se resuelve y se relaja). Lo que esta prueba demuestra para esas 15 248 líneas es el opcode, el `ModRM` y el tamaño de la forma larga; la forma corta solo está comprobada con desplazamientos escritos a mano (`branches`).
- Cuatro vectores del propio archivo quedan sin cubrir a propósito y se informan como tales: `mov` con `ah`/`ch` (3) y un `mov` con un símbolo en la dirección y otro en el inmediato (necesitaría dos relocaciones en una instrucción). Ninguno aparece en el corpus.
- El corpus son los programas de `tests/` y `examples/`. Un programa nuevo con una instrucción distinta puede caer fuera: el generador, no el codificador, decide qué se emite. Por eso `inventory.sh` y `tests/x64` se regeneran a partir de cualquier cambio del generador.
- No se ejecutó ningún ELF: esta máquina es macOS arm64 y no corre binarios de Linux. Que los bytes coincidan con nasm no prueba que el programa final corra; eso lo comprobará CI (paso 6) cuando exista el escritor del ELF.
