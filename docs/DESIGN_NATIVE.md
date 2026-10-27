# Escribir el ELF sin nasm ni ld (paso 1)

Meta de largo plazo: que Copihue no dependa de C ni de `nasm`/`ld`. Este documento cubre el **paso 1**: que el compilador escriba por sí mismo el ejecutable ELF x86-64 del modo hosted (Linux). Hoy `copihue` escribe texto NASM en `output.asm` y llama a `nasm -f elf64` y `ld`.

Estado de esta porción (0.42.3, nada de esto está activo por defecto):

- Hecho: el inventario de abajo, y un codificador de instrucciones (`compiler/native/x64.c`, con relocaciones; ver [DESIGN_NATIVE_X64.md](DESIGN_NATIVE_X64.md)) que coincide byte a byte con `nasm -f elf64` en el 100 % de las instrucciones del inventario (ver [Cobertura](#cobertura-de-la-primera-porción)).
- No hecho: etiquetas y saltos, símbolos de datos, secciones, cabecera ELF, la opción de línea de comandos. Es el resto de este documento.
- El ejecutable producido **no se ha ejecutado** en ningún sitio: la máquina de desarrollo es macOS arm64 y no corre ELF de Linux. Lo verificado es la igualdad de bytes contra nasm, no el comportamiento de un programa.

## 1. Levantamiento

### Método

`tools/native/corpus.sh` compila con `-S` cada programa que `tests/run_tests.sh` ejecuta en modo hosted (los `.lap` de `tests/` y `examples/`, sin `tests/errors`, `freestanding`, `flat`, `boot`, `usermode` ni `ctx`), a `-O0` y a `-O2`: 233 fuentes, 466 archivos `.asm`, 0 fallos de compilación. `tools/native/asminv.c` lee esos archivos con el mismo analizador que usa el codificador, y clasifica cada instrucción en una **forma**: mnemónico más la clase de cada operando (`r64`, `r32`, `r16`, `r8`, `imm`, `imm(sym)`, `label`, `xmm`, y memoria `m<tamaño>[base+índice*escala+símbolo+desplazamiento]`).

```
make native-test                 # prueba contra nasm (genera el corpus y lo compara)
make native-inventory            # regenera el corpus y reimprime este resumen
```

### Totales

| | |
|---|---|
| archivos `.asm` | 466 |
| instrucciones | 181 987 |
| de ellas en el runtime (`_start` y funciones `__*`) | 39 610 |
| de ellas en código de usuario | 142 377 |
| formas distintas | 207 |
| mnemónicos distintos (contando `lock`/`rep` + mnemónico) | 78 |
| directivas | 8 230 |
| etiquetas | 14 839 |
| líneas que el analizador no entiende | 0 |

El runtime (`__print_int`, `__strcmp`, `__strlen`...) se copia en cada programa, así que sus 39 610 instrucciones son unas 85 por archivo repetidas. Cada programa tiene al menos un `jmp`/`call` a etiqueta (466 de 466), de modo que ningún archivo entero se podría ensamblar solo con lo implementado hoy.

### Lo que no es una instrucción

Directivas que el generador hosted emite: `section .text|.data|.bss`, `global _start`, `db` (4 733 etiquetas de datos, casi todas cadenas `str_N: db ...`), `dq` (322; también `dq str_N` y `dq N`), `dd` (4), `resb` (186), `resq` (30), `resd` (18), `resw` (4), `align N` (`align N, db ..`, `align N, resb ..`) y `__frame_f equ N` (una por función, **al final** de la función: referencia hacia adelante).

Formas de símbolo en los operandos: `mov r64, etiqueta` (dirección de cadena o dato), `[etiqueta]`, `[etiqueta+reg*escala+desp]`, `[rel .L42]` (RIP-relativo, 4 veces, solo `movdqu`), saltos y llamadas a etiqueta (locales `.L12`, `.loop` y globales `main`), y `[rbp - __frame_f + N]` / `sub rsp, __frame_f` (constantes `equ`).

### Mnemónicos (hosted, `-O0` y `-O2`)

Salida de `asminv` (columnas: total de ocurrencias, formas distintas).

```
    total  formas  mnemonico
    87596      66  mov            14451  2  push        10346  1  syscall
     8288       1  pop             8268  8  add          5472  1  ret
     5156       1  leave           4316  2  jmp          4230  2  call
     3889      10  cmp             3868  3  xor          3764 10  lea
     2906       3  test            2818  2  inc          2735  4  sub
     1718       1  jz              1419 13  movzx        1319  3  imul
     1134       1  jge             1068  1  jne           932  1  dec
      767       1  jnz              618  1  neg           571  1  jl
      554       3  movsxd           489  3  shl           466  1  div
```

La lista completa de 78 mnemónicos y de las 207 formas con sus conteos está en el [Apéndice A](#apéndice-a-todas-las-formas). Lo que importa para el diseño: `mov` es el 48 % de todo; `push`/`pop`/`syscall`/`ret`/`leave` más el `mov` de registro e inmediato son más de la mitad; SSE2 aparece 53 veces en total (13 `movdqu`, 8 `paddb`...) y en uno o dos archivos por forma; el resto de enteros, atómicos (`lock xadd`, `lock cmpxchg`, `lock add`, `xchg`) y `rep movsb/stosq/stosb` son pocos pero necesarios.

### Comportamientos de nasm que hay que copiar

Medidos con `nasm 3.x` (ver `tests/x64/edge.asm` y `cases.asm`; el CI usa nasm 2.16.01 y `movsxd` y `xchg` difieren, ver abajo):

- `reg, reg` usa la forma «r/m, r» (`89`, `01`, `31`...); `reg, mem` la forma «r, r/m» (`8B`, `03`...).
- ALU con inmediato: `83 /n ib` si cabe en int8; si no, la forma corta del acumulador (`48 05 id`, `3D`...); si no, `81 /n id`. Para `al`, siempre `04 ib`/`3C ib`.
- `mov rax, 60` es `B8 3C 00 00 00` (32 bits, extiende con ceros); `mov rax, -1` es `48 C7 C0 FF FF FF FF`; más de 32 bits sin signo, `48 B8 imm64`.
- `shl r, 1` es `D1 /4`; con otro inmediato `C1 /4 ib`.
- `movsxd rax, eax` se reduce a `cdqe` (`48 98`) en nasm 3.x; nasm 2.x (el del CI) emite `48 63 C0`, y `x64.c` sigue a 2.x. `xchg rax, r` es `90+r`; en `xchg` entre registros el izquierdo va en `ModRM.reg`, como nasm 2.16.01.
- Una `equ` definida **después** de su uso se encoge igualmente: `sub rsp, F` con `F equ 64` es `48 83 EC 40`, con `F equ 400` es `48 81 EC 90 01 00 00`. nasm optimiza en varias pasadas; el codificador debe decidir con el valor final.
- `mov rax, etiqueta` es `movabs` (`48 B8 imm64`, 10 bytes) y deja una relocación `R_X86_64_64`; `[etiqueta]` es `[disp32]` absoluto con SIB (`04 25`) y `R_X86_64_32S`.
- Una instrucción `jmp .x` a la etiqueta siguiente es `EB 00` (rel8); la elección rel8/rel32 es de nasm.

## 2. Diseño

### 2.1 Dos maneras de alimentar el codificador

1. **Texto de entrada** (el camino de esta porción). El generador sigue escribiendo el ensamblador NASM; un ensamblador mínimo lo lee con `x64_parse` y lo codifica con `x64_encode`. Ventaja decisiva: hay un oráculo exacto (`nasm -f elf64` sobre el mismo texto), así que cada paso se prueba byte a byte, y el generador de 21 000 líneas no cambia. El texto sigue disponible con `-S`.
2. **Estructuras**. El generador llama a `x64_encode` con `XInsn` en lugar de `fprintf`. Más rápido y sin analizar texto, pero obliga a reescribir miles de `fprintf` y pierde el oráculo mientras dure la migración.

Se hace 1 y se deja 2 para cuando el camino de texto esté en producción y cubra el 100 % del inventario: `XInsn` ya es la interfaz de `x64_encode`, así que el cambio posterior es mecánico.

### 2.2 Codificación

`x64.c` produce, por instrucción, hasta 15 bytes en el orden: prefijo `lock`/`rep`, `66` de tamaño de operando, prefijo obligatorio SSE, `REX`, opcode (1–3 bytes), `ModRM`, `SIB`, desplazamiento (0/1/4), inmediato (0/1/2/4/8). `REX` solo se emite si hace falta (W, R, X, B, o un registro de 8 bits `spl/bpl/sil/dil`); `ah/ch/dh/bh` con `REX` se rechaza. Las reglas de memoria:

- base `rsp`/`r12` obliga a `SIB`; base `rbp`/`r13` sin desplazamiento obliga a `disp8 = 0`;
- `disp8` si cabe en int8, si no `disp32`;
- sin base y sin índice, `[disp32]` con `SIB 25`; índice sin base, `disp32`;
- índice `rsp` se rechaza; `[reg*2]` sin base se reescribe a `[reg+reg]`, como nasm.

Los símbolos constantes (`equ`) se pliegan en el desplazamiento o el inmediato antes de elegir tamaño. Un símbolo que no es constante produce una relocación (`XReloc`). Una forma que no está en el subconjunto devuelve `0` con el motivo: **nunca codifica mal en silencio** (`tests/x64/reject.asm`, modo `reject` de `tests/x64/run.sh`, lo comprueba: cada línea inválida debe rechazarla `x64.c` y también nasm).

### 2.3 Etiquetas y saltos con relajación

Todo salto `jmp`/`jcc` empieza en su forma corta (`EB rel8`, `7x rel8`) y se relaja a larga (`E9 rel32`, `0F 8x rel32`) hasta el punto fijo: mientras algún desplazamiento no quepa en int8, se agranda ese salto y se recalculan las direcciones. Los saltos solo crecen, así que termina. `call` es siempre rel32 (`E8`). Los tamaños de las demás instrucciones dependen del valor de una `equ` (imm8 o imm32), no de la dirección de una etiqueta, por lo que entran en el mismo bucle sin dependencia circular: se resuelven las `equ` primero (el archivo entero ya está leído), después los saltos.

Las etiquetas locales (`.L12`, `.loop`) pertenecen a la última etiqueta no local; el ensamblador las guarda como `padre.hijo`. Hay que comprobar, contra nasm, que el resultado de la relajación coincide con el suyo en todo el corpus; si hubiera casos en que nasm deja un salto largo sin necesidad, se documentan y se imita.

### 2.4 Secciones y datos

Tres búferes: `.text`, `.data`, `.bss` (solo un tamaño). Las directivas de datos (`db`, `dq`, `dd`, `resb`...) y `align N[, relleno]` se codifican directamente; el relleno de `align` en `.text` (nop de nasm) **no se ha medido todavía** y es un punto de la primera prueba de archivo entero: `align 32` está en los bucles calientes y su efecto en las direcciones es lo que las mediciones del CHANGELOG suponen.

### 2.5 Relocaciones

En un ejecutable estático con dirección base fija **no hace falta registro de relocaciones**: se asignan las direcciones de las tres secciones y cada referencia se parchea directamente.

- `mov r64, etiqueta`: se conserva `movabs` de 10 bytes (idéntico a nasm). Acortarlo a `mov r32, imm32` (5 bytes) cambiaría el tamaño del código y las direcciones; sería una mejora posterior, tras una opción propia y con medición.
- `[etiqueta + ...]`: `disp32` con signo; las direcciones (`0x40xxxx`) caben.
- `dq etiqueta`: 8 bytes absolutos. `[rel .L]`: `rip + disp32`.
- `call`/`jmp` a etiqueta: relativo, ya cubierto por 2.3.

Los objetos `-c` (ELF relocatable para `ld`) necesitarán la misma codificación más una tabla de relocaciones (`R_X86_64_64`, `_32S`, `_PC32`); es otra fase, y tiene ya su propio camino con nasm.

### 2.6 Cabecera ELF

`ET_EXEC`, `EM_X86_64`, `e_entry = _start`. Dos `PT_LOAD` (reparto propuesto; el que produce `ld` hoy no se pudo inspeccionar en esta máquina macOS, y hay que compararlo en Linux antes de M4):

| segmento | permisos | contenido | dirección virtual |
|---|---|---|---|
| 1 | R+X | cabeceras ELF + `.text` | `.text` en `0x401000` (offset de archivo `0x1000`) |
| 2 | R+W | `.data`, luego `.bss` (`p_memsz > p_filesz`) | siguiente página, `vaddr ≡ offset (mod 0x1000)` |

Se mantiene `.text` en `0x401000` (supuesto a confirmar en Linux, por la dirección de `fib` que cita el CHANGELOG) para que las direcciones que citan las mediciones del CHANGELOG (`fib` en `0x4010df`) no se muevan. La alineación del segmento debe ser al menos 32, por el `align 32` de los bucles. Sin cabeceras de sección en la primera versión; luego `.shstrtab`, `.symtab` y `.strtab` mínimos, porque el flujo de medición usa `objcopy -j .text` y los depuradores esperan símbolos. El archivo se escribe con modo `0755`.

### 2.7 Qué queda fuera

Este inventario es solo del modo hosted. `--freestanding`, `--flat`, `--boot`, `bits 16/32`, funciones `naked`/`interrupt` y `asm("...")` de usuario pueden emitir cualquier instrucción (modos de 16 y 32 bits, registros de control, `iret`...). Hasta tenerlos inventariados, el camino nativo debe **rechazar** esos programas y caer al camino actual (nasm + ld), no intentar un subconjunto.

## 3. Orden de implementación

| paso | qué | criterio de aceptación |
|---|---|---|
| M1 (hecho) | codificador de instrucciones sin etiquetas (registros, memoria, `equ`) | `make native-test`: cada instrucción aceptada coincide con nasm byte a byte (y relocación); la entrada inválida se rechaza |
| M2 | etiquetas, saltos con relajación, `call` | cada archivo del corpus: los bytes de `.text` coinciden con los de nasm (con `section .text start=...`); se mide cuántos saltos nasm deja largos |
| M3 | datos: `db dq dd resb`, `align`, símbolos en `mov`/`lea`/`[...]`, `[rel]` | igual, con `.data`; la cobertura del inventario llega al 100 % de las formas hosted |
| M4 | escritor ELF y opción `--native` (nombre por decidir); sin cabeceras de sección | en el CI (Linux): `make test` con la opción en cada nivel `-O0`/`-O2`, mismas salidas doradas |
| M5 | `.symtab`/`.shstrtab`, comparación de `.text` con el de `ld` vía `objcopy -O binary -j .text` | `cmp` idéntico en el banco de `bench/` |
| M6 | la opción pasa a ser el camino por defecto, con el viejo detrás de una opción; después los objetos `-c` | un ciclo de CI verde y las mediciones del CHANGELOG sin cambios |

El paso 1 termina en M5. La dependencia de C (el compilador está escrito en C) es otra meta y no entra aquí.

## Cobertura de la primera porción

Medida con `make native-test` (`tests/x64`) sobre el corpus de 466 archivos; «cubierta» significa que `x64_encode` produce bytes y que esos bytes (y la relocación, si la hay) **coinciden con nasm**.

| | ocurrencias cubiertas | % |
|---|---|---|
| todas | 181 987 de 181 987 | **100 %** |
| instrucciones distintas (símbolos renombrados) | 4 334 de 4 334 | 100 % |
| archivos que se ensamblarían enteros | 0 de 466 | 0 % |

Los 15 248 saltos y llamadas a etiqueta se codifican en forma `rel32` con una relocación contra el símbolo, y los 7 240 operandos con símbolo no constante (`mov r64, etiqueta`, `[etiqueta]`, `[etiqueta+reg*escala]`, `[rel ...]`) con su relocación: es lo que hace nasm con un símbolo externo. Resolver las etiquetas de la misma sección y relajar los saltos es M2; los datos, M3.

Lo que esta cifra **no** dice: que un programa compile con esta ruta (no hay ensamblador ni ELF todavía), ni que el código resultante corra. Además del corpus, `tests/x64` compara con nasm las 344 instrucciones de `edge.asm` (todos los registros `r8`–`r15`, `rsp`/`r12`/`rbp`/`r13` como base, límites de inmediatos, `ah`–`bh`, `movd` con memoria), las 3 728 de `cases.asm` (instantánea del corpus) y 454 saltos con desplazamiento conocido, y comprueba que las 48 líneas de `reject.asm` las rechace `x64.c` (y nasm, en 43 de ellas).

## Apéndice A. Todas las formas

Formas, conteos y ejemplos de `asminv` sobre el mismo corpus. `cod` indica si `x64.c` produce bytes: hoy todas (los símbolos con relocación y los saltos a etiqueta en forma `rel32`; la columna se actualizó a mano con el resultado de `make native-test`, la tabla no se regeneró).

```
    total   runtime   usuario  arch  cod forma                              ejemplo / motivo
    37522      3728     33794   466  si  mov r64,imm                        mov rax, 60
    24580      4660     19920   466  si  mov r64,r64                        mov rdi, rax
    11107      2796      8311   466  si  push r64                           push rbp
    10346      1398      8948   466  si  syscall                            syscall
     8288       932      7356   466  si  pop r64                            pop rbx
     5978         0      5978   264  si  mov r64,m?[b+d]                    mov rax, [rbp-88]
     5472      2330      3142   466  si  ret                                ret
     5156      2330      2826   466  si  leave                              leave
     4709         0      4709   450  si  add r64,imm                        add rsp, 8
     4709         0      4709   448  si  mov r64,imm(sym)                   mov rax, str_0
     4308      1864      2444   466  si  jmp label                          jmp .print_loop
     4158       466      3692   466  si  call label                         call main
     4000         0      4000   269  si  mov m?[b+d],r64                    mov [rbp-88], rax
     3619      1398      2221   466  si  xor r64,r64                        xor rdx, rdx
     3344         0      3344   444  si  push imm                           push 10
     3035         0      3035   290  si  add r64,r64                        add rax, rbx
     2796      2796         0   466  si  inc r64                            inc rdi
     2549         0      2549   165  si  mov r64,m?[b-s+d]                  mov r12, [rbp - __frame_main + 8]
     2085      1398       687   466  si  lea r64,m?[b+d]                    lea rsi, [rbp-1]
     1910       932       978   466  si  test r64,r64                       test rbx, rbx
     1743       466      1277   466  si  cmp r64,r64                        cmp rdi, rax
     1718      1398       320   466  si  jz label                           jz .strcmp_equal
     1402      1398         4   466  si  mov r8,m?[b]                       mov al, [rdi]
     1344         0      1344    93  si  lea r64,m?[s]                      lea rbx, [array]
     1250         0      1250   458  si  sub r64,imm(sym)                   sub rsp, __frame_main
     1134         0      1134   179  si  jge label                          jge .L1
     1071         0      1071   165  si  mov m?[b-s+d],r64                  mov [rbp - __frame_main + 8], r12
     1068       466       602   466  si  jne label                          jne .strcmp_diff
     1067         0      1067    84  si  imul r64,imm                       imul rax, 4
     1061         0      1061   190  si  cmp r64,imm                        cmp rax, 10
     1001       932        69   466  si  movzx r64,r8                       movzx rax, al
      936       932         4   466  si  mov m?[b],r8                       mov [rdi], dl
      932       932         0   466  si  dec r64                            dec rdi
      932       932         0   466  si  test r8,r8                         test al, al
      767       466       301   466  si  jnz label                          jnz .loop
      752       466       286   466  si  sub r64,imm                        sub rsp, 32
      729       466       263   466  si  sub r64,r64                        sub rax, rbx
      618       466       152   466  si  neg r64                            neg rbx
      571       466       105   466  si  jl label                           jl .done
      565       466        99   466  si  mov m8[b+d],imm                    mov byte [rbp-1], 45
      522         0       522   143  si  mov r64,m?[b]                      mov rsi, [rdi]
      467       466         1   466  si  add r8,imm                         add dl, 48
      466       466         0   466  si  cmp m8[b+i*1],imm                  cmp byte [rdi+rax], 0
      466       466         0   466  si  cmp r8,r8                          cmp al, bl
      466       466         0   466  si  div r64                            div rcx
      466       466         0   466  si  jns label                          jns .positive
      464         0       464    75  si  shl r64,imm                        shl rcx, 1
      426         0       426    99  si  js label                           js .Lbounds_error_6
      421         0       421    91  si  mov r64,m?[s]                      mov rax, [iterations]
      380         0       380   118  si  mov m?[b],r64                      mov [rax], rbx
      352         0       352    27  si  mov r32,imm                        mov edi, 0x350F00
      327         0       327    29  si  mov m32[b],r32                     mov dword [rbx], eax
      293         0       293    78  si  movzx r64,m8[b]                    movzx rax, byte [rbx]
      292         0       292    40  si  mov m64[b+d],imm                   mov qword [rbp-24], 0
      290         0       290    35  si  mov m8[b],r8                       mov byte [rcx], al
      280         0       280    87  si  je label                           je .L8
      266         0       266    32  si  movsxd r64,r32                     movsxd rax, eax
      248         0       248    28  si  xor r32,r32                        xor edi, edi
      218         0       218    32  si  movsxd r64,m32[b+d]                movsxd rax, dword [rax+8]
      214         0       214    66  si  imul r64,r64                       imul rax, rbx
      214         0       214    52  si  mov m?[s],r64                      mov [free_ord], rax
      191         0       191    60  si  cqo                                cqo
      191         0       191    60  si  idiv r64                           idiv rbx
      155         0       155    43  si  and r64,imm                        and rsi, -16
      146         0       146    26  si  lea r64,m?[b+i*1+d]                lea rax, [rbp-96+rbx]
      140         0       140    11  si  lea r64,m?[b+s]                    lea rax, [orders+rax]
      137         0       137     7  si  mov m8[i*1+s],imm                  mov byte [sockaddr+r12*1], 0
      129         0       129    24  si  sar r64,imm                        sar rdx, 63
      123         0       123    59  si  jle label                          jle .L6
      119         0       119    43  si  mov m64[b],r64                     mov qword [rbx], rax
      117         0       117    11  si  mov m64[b+i*1+d],r64               mov qword [rbp-24+rbx], rax
      110         0       110    13  si  mov r64,m?[b+i*1+d]                mov rax, [rbp-24+rbx]
      106         0       106    28  si  mov m8[b+i*1+d],r8                 mov byte [rbp-40+rbx], al
      106         0       106    13  si  mov r64,m?[b+i*8+d]                mov rax, [rbp-24+r12*8]
      103         0       103    49  si  jg label                           jg .L1
       96         0        96    29  si  shr r64,imm                        shr r11, 3
       86         0        86    29  si  mov m32[b+d],r32                   mov dword [rbp-8], ecx
       80         0        80    30  si  mov r32,r32                        mov edx, eax
       76         0        76    17  si  cmp m64[b+d],imm                   cmp qword [rbp-48], 0
       76         0        76    28  si  jae label                          jae .L0
       72         0        72    26  si  call r64                           call r12
       70         0        70    14  si  movsxd r64,m32[b]                  movsxd rax, dword [rax]
       66         0        66    26  si  mov r32,m32[b]                     mov eax, dword [rbx]
       64         0        64    24  si  hlt                                hlt
       64         0        64    24  si  test r32,r32                       test eax, eax
       57         0        57    15  si  and r64,r64                        and rax, rbx
       57         0        57    23  si  cld                                cld
       57         0        57    18  si  mov r64,m64[b]                     mov rax, qword [r11]
       48         0        48     8  si  mov m64[b+i*8+d],r64               mov qword [rbp-4800+r12*8], rax
       48         0        48    13  si  mov r32,m?[b]                      mov eax, [rbx]
       47         0        47    22  si  cmp r64,m?[b+d]                    cmp rax, [rbp-8]
       47         0        47    26  si  movzx r64,m8[b+i*1+d]              movzx rax, byte [rbp-8200+rbx]
       46         0        46    12  si  ud2                                ud2
       38         0        38    16  si  imul r64,r64,imm                   imul rax, rax, 1103515245
       38         0        38    16  si  lock xadd m64[b],r64               lock xadd qword [r11], rax
       37         0        37    11  si  add r64,m?[b+d]                    add r12, [rbp-48]
       30         0        30     6  si  mov r32,m?[b+d]                    mov eax, [rbp-8]
       30         0        30    13  si  rep movsb                          rep movsb
       28         0        28    13  si  mov r64,m64[s]                     mov rax, qword [line]
       28         0        28    12  si  pause                              pause
       27         0        27     7  si  mov m8[i*1+s],r8                   mov byte [sockaddr+rax*1], bl
       27         0        27     5  si  movzx r64,m8[i*1+s]                movzx rax, byte [response_buffer+r14*1]
       26         0        26    12  si  movzx r64,m8[b+d]                  movzx rax, byte [rbp-16]
       24         0        24     6  si  ja label                           ja .Lslice_trap_17
       24         0        24     4  si  mov m?[b+d],r8                     mov [rcx+1], al
       24         0        24     4  si  mov r8,m?[b+d]                     mov al, [rbx+1]
       22         0        22    12  si  inc m64[b+d]                       inc qword [rbp-16]
       22         0        22    12  si  jb label                           jb .L3
       22         0        22     5  si  mov m32[b+d],imm                   mov dword [rbp-72], 1
       22         0        22     8  si  mov m8[b+i*1],r8                   mov byte [rsp+r11], al
       22         0        22     8  si  rep stosq                          rep stosq
       22         0        22     8  si  shl r32,r8                         shl eax, cl
       21         0        21     8  si  mov m32[b+i*1+d],r32               mov dword [rbp-80+rbx], eax
       21         0        21    13  si  mov m8[b+d],r8                     mov byte [rcx+8], al
       21         0        21     8  si  mov r32,m?[b+i*1+d]                mov eax, [rbp-80+rbx]
       20         0        20     8  si  mov r32,m?[b+i*4+d]                mov eax, [rbp-80+r12*4]
       19         0        19     4  si  mov m8[s+d],imm                    mov byte [sockaddr+1], 0
       18         0        18     8  si  lea r64,m?[s+d]                    lea rax, [line+64]
       18         0        18     2  si  mov m32[s+d],imm                   mov dword [arr+4], 3
       16         0        16     7  si  cmp r64,m?[s]                      cmp r13, [iterations]
       15         0        15     4  si  mov m64[s],r64                     mov qword [hb], r12
       14         0        14     5  si  add r64,m?[s]                      add rax, [filled]
       13         0        13     3  si  popfq                              popfq
       13         0        13     3  si  pushfq                             pushfq
       12         0        12     3  si  lea r64,m?[b+i*8+d]                lea rdi, [rbp-40+rax*8]
       12         0        12     6  si  mov m16[b+d],r16                   mov word [rbp-16], cx
       12         0        12     6  si  sete r8                            sete al
       11         0        11     9  si  lea r64,m?[b+i*2]                  lea r13, [r12+r12*2]
       11         0        11     9  si  lock add m64[s],imm                lock add qword [line], 1
       10         0        10     4  si  movzx r64,m16[b+d]                 movzx rax, word [rbp-16]
        9         0         9     8  si  lock add m64[s+d],imm              lock add qword [line+64], 1
        9         0         9     4  si  mov m16[b],r16                     mov word [rcx], ax
        9         0         9     8  si  mov r64,m64[s+d]                   mov rbx, qword [line+64]
        9         0         9     7  si  sar r64,r8                         sar rax, cl
        8         0         8     2  si  jmp r64                            jmp rax
        8         0         8     2  si  mfence                             mfence
        8         0         8     3  si  mov r32,m?[i*4+s]                  mov eax, [array+r15*4]
        8         0         8     6  si  movsx r64,r8                       movsx rax, al
        8         0         8     1  si  paddb xmm,xmm                      paddb xmm0, xmm2
        8         0         8     4  si  setz r8                            setz al  ; Set al to 1 if rax was 0
        7         0         7     4  si  cmp m64[s],imm                     cmp qword [pa], 0  ; volatile
        6         0         6     3  si  lock cmpxchg m64[b],r64            lock cmpxchg qword [r11], r10
        6         0         6     6  si  mov m8[s],imm                      mov byte [sockaddr], 2
        6         0         6     4  si  setg r8                            setg al
        5         0         5     3  si  cmp m64[b+d],r64                   cmp qword [rbp-8], r15
        5         0         5     2  si  lea r64,m?[i*8+s]                  lea rax, [seen+r12*8]
        5         0         5     5  si  mov m32[b+i*4+d],r32               mov dword [rbp-80+r14*4], eax
        5         0         5     3  si  mov r64,m?[i*8+s]                  mov rax, [slots+rax*8]
        5         0         5     2  si  mov r8,imm                         mov al, 7
        5         0         5     1  si  movdqu xmm,m?[b]                   movdqu xmm0, [rax]
        5         0         5     2  si  movzx r64,m16[b]                   movzx rax, word [rbx]
        5         0         5     2  si  rep stosb                          rep stosb
        4         0         4     2  si  clc                                clc
        4         0         4     2  si  cpuid                              cpuid
        4         0         4     2  si  jnc label                          jnc .L0
        4         0         4     2  si  lock xadd m32[b],r32               lock xadd dword [r11], eax
        4         0         4     1  si  mov m16[b+d],imm                   mov word [rbp-32], 100
        4         0         4     1  si  mov m16[b+i*1+d],r16               mov word [rbp-32+rbx], ax
        4         0         4     4  si  mov m?[s+d],r64                    mov [g+8], rax
        4         0         4     4  si  mov r64,m?[s+d]                    mov rax, [g+8]
        4         0         4     1  si  movd xmm,r32                       movd xmm1, eax
        4         0         4     1  si  movdqu m?[b],xmm                   movdqu [rax], xmm0
        4         0         4     1  si  movdqu xmm,m?[rip+s]               movdqu xmm2, [rel .L42]
        4         0         4     1  si  movq r64,xmm                       movq rax, xmm3
        4         0         4     4  si  not r64                            not rax
        4         0         4     1  si  pshufd xmm,xmm,imm                 pshufd xmm1, xmm1, 0
        4         0         4     1  si  punpcklbw xmm,xmm                  punpcklbw xmm1, xmm1
        4         0         4     1  si  punpcklwd xmm,xmm                  punpcklwd xmm1, xmm1
        4         0         4     1  si  pxor xmm,xmm                       pxor xmm2, xmm2
        4         0         4     2  si  setc r8                            setc al
        4         0         4     4  si  setl r8                            setl al
        4         0         4     2  si  stc                                stc
        4         0         4     3  si  sub r64,m?[b+d]                    sub rax, [rbp-96]
        4         0         4     2  si  xchg m64[b],r64                    xchg qword [r11], rax
        3         0         3     3  si  mov m32[s],imm                     mov dword [arr], 1
        3         0         3     1  si  mov m64[s+d],imm                   mov qword [recs_k+8], 5
        3         0         3     1  si  mov m8[b+i*1+d],imm                mov byte [rbp-32+r12*1], 1
        3         0         3     3  si  shl r64,r8                         shl rax, cl
        2         0         2     1  si  add m8[i*1+s],imm                  add byte [buf+r12*1], 1
        2         0         2     2  si  add r32,r32                        add eax, r10d
        2         0         2     1  si  add r64,m?[b]                      add r14, [rax]
        2         0         2     1  si  cmp m64[b],imm                     cmp qword [rax], 0
        2         0         2     2  si  lea r64,m?[b+i*4]                  lea rax, [r12+r12*4]
        2         0         2     1  si  lock cmpxchg m64[s],r64            lock cmpxchg qword [enq], r10
        2         0         2     2  si  lock xadd m64[s],r64               lock xadd qword [cell], rax
        2         0         2     2  si  mov m32[i*4+s],r32                 mov dword [array+r13*4], eax
        2         0         2     2  si  mov m64[b],imm                     mov qword [r12], 0
        2         0         2     2  si  mov m64[i*8+s],r64                 mov qword [slots+rax*8], r13
        2         0         2     1  si  movdqa xmm,xmm                     movdqa xmm4, xmm0
        2         0         2     2  si  movsx r64,r16                      movsx rax, ax
        2         0         2     1  si  movzx r32,r8                       movzx eax, r15b
        2         0         2     1  si  movzx r64,m16[b+i*1+d]             movzx rax, word [rbp-32+rbx]
        2         0         2     1  si  movzx r64,m16[b+i*2+d]             movzx rax, word [rbp-32+r12*2]
        2         0         2     2  si  movzx r64,r16                      movzx rax, ax
        2         0         2     1  si  paddq xmm,xmm                      paddq xmm3, xmm4
        2         0         2     1  si  psadbw xmm,xmm                     psadbw xmm4, xmm2
        2         0         2     1  si  psrldq xmm,imm                     psrldq xmm3, 8
        1         0         1     1  si  lea r64,m?[b+i*8]                  lea rax, [r12+r12*8]
        1         0         1     1  si  mov m16[s+d],imm                   mov word [w+4], 65535
        1         0         1     1  si  mov m32[b],imm                     mov dword [r12], 99
        1         0         1     1  si  mov m32[i*4+s+d],r32               mov dword [arr+4+r12*4], eax
        1         0         1     1  si  mov m64[s],imm                     mov qword [recs_k], 4
        1         0         1     1  si  movzx r32,m8[i*1+s]                movzx eax, byte [src+r12*1]
        1         0         1     1  si  movzx r64,m16[i*2+s]               movzx rax, word [w+rax*2]
        1         0         1     1  si  or r64,imm                         or rax, 1
        1         0         1     1  si  or r64,r64                         or rax, rbx
        1         0         1     1  si  xor r64,imm                        xor rax, 15
```
