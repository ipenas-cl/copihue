# Escribir el ELF sin nasm ni ld (paso 1)

Meta de largo plazo: que Copihue no dependa de C ni de `nasm`/`ld`. Este documento cubre el **paso 1**: que el compilador escriba por sí mismo el ejecutable ELF x86-64 del modo hosted (Linux). Desde M6, `copihue` genera el texto NASM en memoria, lo ensambla con `compiler/native` y escribe el ELF (ejecutable u objeto `-c`) sin llamar a `nasm` ni a `ld`; el camino viejo (`output.asm`, `nasm -f elf64`, `ld`) queda detrás de `--nasm`.

Estado de esta porción (0.42.5; M6 hecho: la ruta nativa es la de por defecto):

- Hecho: el inventario de abajo, y un codificador de instrucciones (`compiler/native/x64.c`, con relocaciones; ver [DESIGN_NATIVE_X64.md](DESIGN_NATIVE_X64.md)) que coincide byte a byte con `nasm -f elf64` en el 100 % de las instrucciones del inventario (ver [Cobertura](#cobertura-de-la-primera-porción)).
- Hecho (M2): un ensamblador de programas completos, `compiler/native/asm.c`, que resuelve etiquetas, relaja saltos como nasm y codifica `call`; produce los bytes de `.text` de todos los archivos del corpus idénticos a `nasm -f elf64` (ver [M2](#m2-etiquetas-saltos-con-relajación-y-call-compilernativeasmhc)).
- Hecho (M3): `asm.c` también arma `.data`, `.rodata` y `.bss` (datos, `align`, `times`, símbolos de datos en `mov`/`lea`/`[...]`/`[rel]` y en `dq`/`dd`) con los mismos bytes, tamaños y relocaciones (offset, tipo, símbolo, addendo) que `nasm -f elf64` en todo el corpus (ver [M3](#m3-datos-compilernativeasmhc)).
- Hecho (M4): `asm.c` entrega su tabla de símbolos; `compiler/native/elf.{h,c}` escribe el ELF64 `ET_EXEC` con relocaciones parcheadas; `copihue --native` genera el ejecutable hosted sin `nasm` ni `ld` y rechaza (sin caer al otro camino) lo que el ensamblador nativo no cubre; hay pruebas de estructura en cualquier sistema y una pasada de `tests/run_tests.sh --native` en el CI (ver [M4](#m4-escritor-elf-y-opción---native-compilernativeelfhc)).
- Hecho (M5): el ELF de `--native` lleva `.symtab`, `.strtab`, `.shstrtab` y cabeceras de sección mínimas, con una tabla de símbolos que coincide (nombre, enlace, sección, dirección, tamaño) con las etiquetas del objeto de nasm en todo el corpus; y hay dos scripts para Linux que comparan con `ld` y leen el archivo con `readelf`/`objdump` (ver [M5](#m5-symtab-strtab-shstrtab-y-comparación-de-text-con-ld)). Al abrir M6 se informó de una primera ejecución de `make native-text-test` en Linux: 456 comparaciones con `.text` igual al de nasm + ld, 437 de ellas solo al volver a enlazar con los datos donde los pone `--native` (cifras que se recibieron, no medidas en la sesión de M6).
- Hecho (M6): `--native` pasa a ser el camino por defecto, sin `nasm` ni `ld` (el viejo, con `--nasm`); el texto ya no pasa por `output.asm`; hay objetos `-c` nativos (`ET_REL`); la suite por defecto corre por la ruta nativa y el camino viejo tiene su propia pasada (ver [M6](#m6-el-camino-nativo-por-defecto-y-los-objetos--c)).
- No hecho: **ejecutar** en esta máquina un archivo producido: la máquina de desarrollo es macOS arm64 y no corre ELF de Linux. Lo que se verifica aquí es la igualdad de bytes contra nasm y la estructura del ELF; que el programa corra lo dice el CI en Linux.

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

Cada `jmp`/`jcc` se decide entre su forma corta (`EB rel8`, `7x rel8`) y la larga (`E9 rel32`, `0F 8x rel32`) por varias pasadas sobre el archivo, como nasm; el algoritmo exacto, que **no** es «empezar corto y solo crecer» (eso se probó y falla en 5 programas), está en [M2](#m2-etiquetas-saltos-con-relajación-y-call-compilernativeasmhc). `call` es siempre rel32 (`E8`). Los tamaños de las demás instrucciones dependen del valor de una `equ` (imm8 o imm32), no de la dirección de una etiqueta, por lo que no hay dependencia circular: se leen las `equ` primero (el archivo entero ya está leído), después se resuelven los saltos.

Las etiquetas locales (`.L12`, `.loop`) pertenecen a la última etiqueta no local; el ensamblador las guarda como `padre.hijo`. Hay que comprobar, contra nasm, que el resultado de la relajación coincide con el suyo en todo el corpus; si hubiera casos en que nasm deja un salto largo sin necesidad, se documentan y se imita.

### 2.4 Secciones y datos

Tres búferes: `.text`, `.data`, `.bss` (solo un tamaño). Las directivas de datos (`db`, `dq`, `dd`, `resb`...) y `align N[, relleno]` se codifican directamente; el relleno de `align` en `.text` se midió en M2: `nasm` sin `%use smartalign` rellena con `0x90` de un byte (comprobado con `-f bin` para los 31 tamaños de hueco hasta 32, y en `-f elf64` con los programas del corpus y de `asm_cases.asm`), no con nops largos. `align 32` está en los bucles calientes (`o2_eqfall`) y `align 16` delante de las tablas de bytes de `o2_mem`.

### 2.5 Relocaciones

En un ejecutable estático con dirección base fija **no hace falta registro de relocaciones**: se asignan las direcciones de las tres secciones y cada referencia se parchea directamente.

- `mov r64, etiqueta`: se conserva `movabs` de 10 bytes (idéntico a nasm). Acortarlo a `mov r32, imm32` (5 bytes) cambiaría el tamaño del código y las direcciones; sería una mejora posterior, tras una opción propia y con medición.
- `[etiqueta + ...]`: `disp32` con signo; las direcciones (`0x40xxxx`) caben.
- `dq etiqueta`: 8 bytes absolutos. `[rel .L]`: `rip + disp32`.
- `call`/`jmp` a etiqueta: relativo, ya cubierto por 2.3.

Los objetos `-c` (ELF relocatable para `ld`) necesitarán la misma codificación más una tabla de relocaciones (`R_X86_64_64`, `_32S`, `_PC32`); es otra fase, y tiene ya su propio camino con nasm.

### 2.6 Cabecera ELF

`ET_EXEC`, `EM_X86_64`, `e_entry = _start`. Dos `PT_LOAD` (reparto propuesto; el que produce `ld` no se pudo inspeccionar en esta máquina macOS, y sigue sin compararse con `ld` en Linux; lo que M4 implementó está en [M4](#m4-escritor-elf-y-opción---native-compilernativeelfhc), con una diferencia respecto de esta tabla: `.rodata` va al final del segmento 1):

| segmento | permisos | contenido | dirección virtual |
|---|---|---|---|
| 1 | R+X | cabeceras ELF + `.text` | `.text` en `0x401000` (offset de archivo `0x1000`) |
| 2 | R+W | `.data`, luego `.bss` (`p_memsz > p_filesz`) | siguiente página, `vaddr ≡ offset (mod 0x1000)` |

Se mantiene `.text` en `0x401000` (supuesto a confirmar en Linux, por la dirección de `fib` que cita el CHANGELOG) para que las direcciones que citan las mediciones del CHANGELOG (`fib` en `0x4010df`) no se muevan. La alineación del segmento debe ser al menos 32, por el `align 32` de los bucles. Sin cabeceras de sección en la primera versión; luego `.shstrtab`, `.symtab` y `.strtab` mínimos, porque el flujo de medición usa `objcopy -j .text` y los depuradores esperan símbolos. El archivo se escribe con modo `0755`.

### 2.7 Qué queda fuera

Este inventario es solo del modo hosted. `--freestanding`, `--flat`, `--boot`, `bits 16/32`, funciones `naked`/`interrupt` y `asm("...")` de usuario pueden emitir cualquier instrucción (modos de 16 y 32 bits, registros de control, `iret`...). Hasta tenerlos inventariados, el camino nativo debe **rechazar** esos programas, no intentar un subconjunto. M4 decidió rechazar con un error y **no** caer en silencio al camino de nasm + ld (ver M4, «Decisiones»). Desde M6, sin opciones esos programas se construyen con nasm + ld **con un aviso** (no en silencio), y solo `--native` los rechaza (ver M6).

## 3. Orden de implementación

| paso | qué | criterio de aceptación |
|---|---|---|
| M1 (hecho) | codificador de instrucciones sin etiquetas (registros, memoria, `equ`) | `make native-test`: cada instrucción aceptada coincide con nasm byte a byte (y relocación); la entrada inválida se rechaza |
| M2 (hecho) | etiquetas, saltos con relajación, `call` | cada archivo del corpus: los bytes de `.text` coinciden con los de nasm (con `section .text start=...`); se mide cuántos saltos nasm deja largos |
| M3 (hecho) | datos: `db dq dd resb`, `align`, símbolos en `mov`/`lea`/`[...]`, `[rel]` | igual, con `.data`, `.rodata` y `.bss`; la cobertura del inventario llega al 100 % de las formas hosted |
| M4 (hecho, sin ejecutar) | escritor ELF y opción `--native` | en el CI (Linux): `make native-run-test` (`tests/run_tests.sh --native`) con la opción en cada nivel `-O0`/`-O2`, mismas salidas doradas. Lo que se pudo comprobar en macOS es la estructura (`make native-elf-test`); la ejecución espera al CI |
| M5 (hecho; la comparación con `ld` sin ejecutar) | `.symtab`/`.strtab`/`.shstrtab` con cabeceras de sección mínimas; comparación de `.text` con el de `ld` vía `objcopy -O binary -j .text` | estructura y símbolos iguales a los de nasm en los 472 programas (`make native-elf-test`); en el CI, `make native-readelf-test` y `make native-text-test` (`cmp` sobre `tests/` y `examples/`, no sobre `bench/`: ver M5) |
| M6 (hecho; el ciclo de CI verde está pendiente) | la opción pasa a ser el camino por defecto, con el viejo detrás de una opción (`--nasm`); después los objetos `-c` | un ciclo de CI verde y las mediciones del CHANGELOG sin cambios. En esta máquina: `make native-test` (incluye los objetos `-c` contra nasm) y la parte de `make test` que no ejecuta nada |

El paso 1 termina en M5. La dependencia de C (el compilador está escrito en C) es otra meta y no entra aquí.

### M2. Etiquetas, saltos con relajación y `call` (`compiler/native/asm.{h,c}`)

M2 es una biblioteca y sus pruebas. No la usa `copihue`, que sigue escribiendo `output.asm` y llamando a `nasm` y `ld`. Entra el texto NASM de un programa hosted completo y salen los bytes de `.text`, las relocaciones que quedan y las cifras de saltos. Los datos son M3.

**Algoritmo.**

1. *Lectura*: una pasada previa junta todas las `equ` (`__frame_f equ N` está después de su uso, así que `x64_parse` debe recibir el valor final); otra clasifica cada línea con `x64_classify` y arma la lista de ítems de `.text`: instrucción ya codificada con `x64_encode` (su tamaño no depende de ninguna dirección), rama a etiqueta, `db`/`dw`/`dd`/`dq` y `align`. Las etiquetas locales (`.L12`) se guardan como `padre.hijo`, con el último nombre no local (en `.text` o en datos) como padre; una `equ` no cambia el padre.
2. *Relajación*: se imita lo que hace nasm, que decide cada salto en varias pasadas. En cada pasada se recorren los ítems en orden y cada `jmp`/`jcc` se decide **de nuevo**: rel8 si el desplazamiento cabe en int8, medido desde el final de la forma corta (`destino − (dirección + 2)`); si no, rel32 (`E9`, 5 bytes; `0F 8x`, 6). La dirección del salto es la de esta pasada; la del destino es la de esta pasada si la etiqueta ya quedó atrás, y la de la pasada anterior si está adelante (nasm define las etiquetas a medida que avanza). En la **primera** pasada una etiqueta adelante es desconocida y el salto a ella es corto (nasm es optimista); un salto hacia atrás se mide normalmente. También en la primera pasada una instrucción con operando de memoria que nombra una `equ` definida más abajo (`mov [rbp - __frame_f + 8], rax`) ocupa su forma disp32, porque nasm aún no conoce el valor; en las siguientes, la final. Los tamaños pueden ir en ambos sentidos entre pasadas (un salto largo puede volver a corto). Se termina cuando una pasada deja todas las direcciones como estaban (en el corpus, hasta 7 pasadas). `call` es siempre rel32; `jmp short` que al final no cabe es error; `jmp near` fuerza rel32; el relleno de `align` se recalcula en cada pasada.
3. *Emisión*: con las direcciones finales se escriben los bytes. Un `[rel etiqueta]` o una rama a una etiqueta de `.text` se resuelve (`destino + addend − dirección del campo`). Una referencia absoluta a una etiqueta de `.text` (`mov rax, f`) deja una relocación contra la sección `.text` con la dirección de la etiqueta como addendo (también si la etiqueta es `global`: nasm hace lo mismo). Una referencia a un símbolo de `.data`/`.bss` deja una relocación contra la etiqueta con su sección; su addendo depende del orden de los datos y se calcula en M3.

**De dónde sale el modelo de nasm (y qué no se sabe).** No se leyó el código fuente de nasm; el modelo se calibró contra la salida de nasm 3.02. Primero «empezar todo corto y solo crecer» (el mínimo punto fijo): 461 de 466 programas iguales; los 5 distintos tenían un `jmp` que nasm deja largo aunque con el código final cabría corto, porque en una pasada intermedia el código era mayor. Se probaron 16 combinaciones de las cuatro reglas: ¿la forma disp32 de la primera pasada afecta a los operandos de memoria?, ¿y a los inmediatos (`sub rsp, __frame_f`)?, ¿en la primera pasada todo salto es corto o solo los que miran adelante?, ¿un salto que fue largo se queda largo? Solo una da 466 de 466: disp32 sí en memoria, no en inmediatos, adelante-corto/atrás-medido (con «todo corto» también da 466, no se distinguen con este corpus), y **sin** «largo para siempre» (con él fallan entre 5 y 39). No se comprobó con nasm 2.16.01 (el del CI) por no haberlo en esta máquina; si el CI difiere, el síntoma será un programa con un salto distinto y el diagnóstico de `test_asm` nombra la línea.

**Se acepta.** `section .text|.data|.bss` (nada más en la línea), `global NOMBRE` (debe estar definido), etiquetas (`name:`, `.local:`, y `name: instrucción`), `equ` con valor entero simple, `align N` (potencia de 2 hasta 4096) en `.text`, con relleno `0x90`, o `align N, db B` con ese byte, `db`/`dw`/`dd`/`dq` con enteros en `.text` (el programa `extern_data` tiene un `dq 42` en `.text`), y todo lo que `x64_encode` cubre. En M2, `.data` y `.bss` solo se reconocían; desde M3 se codifican (ver M3, que también cambia algunas cosas de esta lista: `times`, `.rodata` y `db`/`dq` con etiqueta en `.text` ya no se rechazan).

**Se rechaza con mensaje y número de línea.** Cualquier directiva que no sea de la lista (`extern`, `times`, `bits`, `%define`, `org`, `incbin`...), `section` con atributos (`start=`, `align=`), una instrucción que `x64_encode` no cubre, una etiqueta definida dos veces, un símbolo no definido en un salto, un salto a una etiqueta de datos, una rama con desplazamiento (`jmp f+4`), `call short`, instrucciones en `.data`/`.bss`, `db`/`dq` con cadenas o con una etiqueta en `.text`, un `equ` que no sea un entero (`1 + 2`), `global` con varios nombres o de un nombre no definido, y un `align` con relleno `resb` en `.text`. Fuera de alcance: los modos `--freestanding`, `--flat`, `--boot` (ver 2.7), el formato de objeto ELF, el escritor del ejecutable y las direcciones de los datos.

**Prueba (`tests/x64/run_asm.sh`, que `make native-test` ejecuta al final de `run.sh`; solo esta parte: `make native-asm-test`).** `test_asm` (`tests/x64/test_asm.c`) toma cada programa, lo ensambla con `nasm -f elf64`, lee el `.o` con un lector ELF propio (secciones, `.rela.text`, `.symtab`; no usa `objdump` ni `readelf`) y exige los mismos bytes de `.text` y las mismas relocaciones: desplazamiento y tipo siempre; si el destino está en `.text`, también la sección y el addendo; si está en datos, la sección (el addendo es de M3). `section .text start=...` no existe en `-f elf64` (es del formato `bin`); las direcciones son relativas al inicio de `.text` (en un `.o` empieza en 0 y todo salto es relativo, así que no cambia la comparación). Los saltos cortos y largos que deja nasm se cuentan con el recorrido de `asm.c`: como los bytes son idénticos, son los mismos. Cuando un programa difiere, `test_asm` vuelve a correr nasm con listado y nombra los saltos que nasm decidió distinto (línea y forma). Entradas: el corpus (466 archivos), 22 programas pequeños propios en `asm_cases.asm` (límites de ±127/128 en ambos sentidos para `jmp` y `jcc`, cascadas de relajación, locales con el mismo nombre en dos funciones, `align`, `equ` hacia adelante, todos los códigos de condición, `short`/`near`, referencias a `.text` y a datos), y 38 programas de `asm_reject.asm` que `asm.c` debe rechazar (no se exige que nasm también los rechace: p. ej. `times` es válido para nasm). Con nasm 3 cada `movsxd reg, reg` se entrega a nasm como los bytes de nasm 2.x (`48 63 ..`), porque nasm 3 lo acorta a `cdqe` y `x64.c` sigue a 2.x (el del CI); es la única adaptación a la versión de nasm.

**Resultado (nasm 3.02, macOS arm64).** Corpus: 466 de 466 programas con `.text` y relocaciones idénticos a nasm; 22 de 22 casos propios; 38 de 38 rechazos. Saltos a etiqueta en el corpus: `jmp` 4 308 (4 008 cortos, 300 largos), `jcc` 6 782 (6 110 cortos, 672 largos), `call` 4 158 (siempre rel32). De los 11 090 `jmp`/`jcc`, **nasm deja 972 largos (8,76 %) y 10 118 cortos (91,24 %)**; 110 de los 466 programas tienen al menos un salto largo. Con la disposición final de nasm, ninguno de esos 972 cabría en rel8 (la cuenta de `long_fits` es 0): los largos lo son por distancia, no por un resto de las pasadas. Quedan 7 236 relocaciones en `.text` (referencias a cadenas y datos, `mov r64, etiqueta`). Pasadas de relajación: hasta 7 en un programa.

### M3. Datos (`compiler/native/asm.{h,c}`)

M3 sigue siendo una biblioteca y sus pruebas; `copihue` no la usa. A lo que M2 devolvía (`.text` y sus relocaciones) se suman los bytes de `.data` y `.rodata`, el tamaño de `.bss`, la alineación de cada sección y las relocaciones de las tres secciones con contenido. `AsmOut` pasa a tener `sec[ASM_NSEC]` (`.text`, `.data`, `.bss`, `.rodata`), cada una con `bytes`, `size`, `align`, `present` y su lista de relocaciones.

**Algoritmo.** Los datos no tienen relajación: su tamaño no depende de ninguna dirección, así que se codifican al leer.

1. *Lectura*: cada sección con datos tiene un búfer y una posición (`.bss`, solo la posición). Una etiqueta en `.data`/`.rodata`/`.bss` vale la posición de su sección en ese momento (`x: align 8` pone `x` antes del relleno, como nasm). Una sección puede abrirse varias veces y sigue donde quedó. `db dw dd dq` escriben cada valor en little endian; un valor que es una etiqueta (`dq str_0`, `dd f`, `dq tab+8`) escribe ceros y deja una relocación pendiente `(sección, offset, tipo, etiqueta, addendo)`; `dq` da `R_X86_64_64` y `dd` `R_X86_64_32`. `times N` repite la directiva de datos que sigue. `align N[, db B | resb 1]` rellena con `B` (con `0x90` si no se dice, también en `.data` y `.rodata`; medido) o, en `.bss`, reserva. Todo `align N` sube la alineación de su sección a `N` si es mayor (medido: nasm lo hace en `.text`, `.data`, `.rodata` y `.bss`; por omisión `.text` 16 y las otras 4). Un `equ` en un valor de datos se pliega, también el que se define después.
2. *Relajación de `.text`*: la de M2, sin cambios.
3. *Relocaciones*: ya con las direcciones finales de `.text`, cada relocación (del código o de los datos) apunta al **símbolo de sección** del destino (`.text`, `.data`, `.rodata` o `.bss`), también si la etiqueta es `global` (medido en nasm 3.02: `mov rdx, gd` con `global gd` en `.data` sigue siendo `.data+offset`), y su addendo es el addendo de la instrucción más el offset de la etiqueta en su sección. Ejemplos medidos: `[rel d2]` con `d2` en offset 4 → `R_X86_64_PC32 .data +0` (el -4 propio del PC32 y el 4 de la etiqueta); `cmp qword [rel q], 1` → `PC32 .data -5`; `mov rcx, [d2+8]` con `d2` en el offset 4 de `.data` → `R_X86_64_32S .data +12`. Las relocaciones de cada sección van en el orden del fuente. Un `PC32` hacia `.text` desde `.text` se resuelve sin relocación, como en M2.

**Se acepta (además de M2).** Secciones `.text`, `.data`, `.rodata`, `.bss`, sin atributos. En `.data`/`.rodata`/`.text`: `db dw dd dq` con enteros decimales o `0x` (sin octal ni `0b`; `010` es 10, como nasm), con signo, y con un tope por tamaño (`dq` llega a `2^64-1`), con una constante `equ`, o (solo `dd` y `dq`) con una etiqueta y un `+N`/`-N`. En `.bss`: `resb resw resd resq` con un entero, y `align`. `times N` (0 a 16 777 216) delante de una directiva de datos o `res*`. `align N, resb 1` en `.bss`, `align N` y `align N, db B` en las demás. Tope de 1 GiB por sección. No hay cadenas en `db`.

**Se rechaza con mensaje y número de línea (47 programas nuevos en `asm_reject.asm`).** Cadenas y caracteres (`db "a"`, `db 'a'`); una etiqueta en `db`/`dw`; sumas de dos etiquetas, `etiqueta*N`, `N+etiqueta`; reales, binarios (`0b101`), `0X` en mayúscula (no se pudo comprobar con nasm 2.16.01); valores fuera del tamaño; comas sobrantes; etiquetas indefinidas; datos con contenido o `resb` en la sección equivocada (`db` en `.bss`, `resb` en `.data`/`.text`); `resb` con cuenta no entera o negativa; `align` con relleno de otro tipo (`align 8, db 1` en `.bss`, `align 8, resb 1` fuera de `.bss`, `align 8, resb 2`); `times` con cuenta no entera, negativa o enorme, con instrucciones (`times 4 nop`) o anidado; instrucciones en `.data`/`.rodata`/`.bss`; `section` con atributos (`.rodata align=16`). `extern`, `bits`, `%define`, `org`, `incbin` y las demás directivas siguen rechazadas.

**Prueba.** `tests/x64/run_asm.sh` (parte de `make native-test`; sola, `make native-asm-test`). `test_asm` lee el `.o` de nasm con `tests/x64/nobj.h` (lector ELF propio de las cuatro secciones, sus `.rela.*` y `.symtab`; `tests/x64/dump_obj.c` lo imprime y sirvió para medir nasm) y exige: los mismos bytes de `.text`, `.data` y `.rodata`, el mismo tamaño de `.bss` y las mismas relocaciones de las tres secciones (offset, tipo, símbolo y addendo; ahora también el addendo de las que van a datos). Además cuenta, y solo imprime, las secciones cuya alineación difiere de la de nasm (no es obligatorio porque no se pudo verificar con nasm 2.16.01). `tools/native/datainv.c` (con `make native-inventory`, y al final de `run_asm.sh`) cuenta, con `x64_classify`/`x64_parse` y sin usar `asm.c`, las formas de directivas de datos y los operandos que nombran una etiqueta de datos.

**Resultado (nasm 3.02, macOS arm64).**

| | |
|---|---|
| corpus | 466 de 466 archivos con bytes de `.text`, `.data`, `.rodata`, tamaño de `.bss` y relocaciones idénticos a nasm |
| `.text` | 630 277 bytes, 7 236 relocaciones |
| `.data` | 54 700 bytes en 448 programas, 2 relocaciones (`dq str_0`) |
| `.bss` | 3 465 120 bytes en 104 programas |
| `.rodata` | 0: el generador hosted no la usa; la cubren solo los casos propios |
| casos propios | 35 programas (`asm_cases.asm`, 13 nuevos de M3) idénticos a nasm |
| rechazos | 85 programas (`asm_reject.asm`, 47 nuevos) rechazados |
| alineación de sección | 0 diferencias con nasm 3.02 en el corpus y en los casos |

**Cobertura del inventario hosted (medida).** `datainv` encuentra en el corpus 6 848 directivas (1 382 `equ`) y **ninguna** fuera de `section`, `global`, `equ` y las de datos; 16 formas de directivas de datos (la más frecuente `db` con lista de enteros, 2 543 líneas; `db` entero 2 190; `dq` entero 318; `resb` 186; `align N, resb` 26; `align N, db 0` 10; `dq etiqueta` 2...) y 29 formas de instrucción con un operando que nombra una etiqueta de datos (7 152 ocurrencias; `mov r64, etiqueta` 4 709, `lea r64, [etiqueta]` en `.bss` 1 231...). Los 466 archivos se ensamblan enteros y salen idénticos, así que **todas** esas líneas están cubiertas: **100 %** de las formas hosted (antes, con `x64.c` solo, 0 de 466 archivos enteros). «Cubierta» es: `asm.c` la acepta y lo que produce es igual a lo de nasm; no es que el programa corra.

**Lo que no se verificó.** (1) **nasm 2.16.01** (el del CI) no está en esta máquina: las reglas medidas aquí (relleno `0x90` de `align` en datos, símbolo de sección también para `global`, `align` que sube la alineación de la sección, `R_X86_64_32` de `dd etiqueta`, advertencias de `align` en `.bss`) son de nasm 3.02. Si el CI difiere, `test_asm` nombra el programa y la relocación; la alineación de sección solo se imprime. (2) `.rodata` no sale en el corpus: su comportamiento solo se probó con los casos propios. (3) Ningún ejecutable producido con esto se ha ejecutado. (4) No había tabla de símbolos: `asm.c` no devolvía qué etiquetas son `global` ni sus offsets (resuelto en M4). (5) Los límites (1 GiB, 16 777 216 repeticiones) son elecciones de esta biblioteca, no de nasm.

### M4. Escritor ELF y opción `--native` (`compiler/native/elf.{h,c}`)

M4 une lo anterior en un ejecutable. `copihue --native programa.lap -o salida` escribe el mismo texto NASM de siempre en `output.asm`, lo ensambla con `asm.c` (M2 + M3), escribe el ELF con `elf.c` y borra `output.asm`; no llama a `nasm` ni a `ld`. Si el ensamblador nativo no acepta el texto, el error nombra la línea y deja `output.asm` (igual que cuando falla nasm). No es el camino por defecto.

**Tabla de símbolos (`asm.h`).** `AsmOut` trae ahora `sym[]` (`AsmSym`: nombre completo, sección, offset **final** —después de la relajación—, indicador `global`, línea), ordenada por sección, offset y nombre, y `asm_find_sym`. Incluye todas las etiquetas del programa, no solo las `global` (las locales son `padre.hijo`, como en el `.symtab` de nasm). Prueba: `test_asm` compara, para cada programa, cada símbolo definido del `.symtab` de nasm (sección, valor, `GLOBAL`/`LOCAL`) con el de `asm.c` y exige que `asm.c` no tenga ninguno más: 472 de 472 programas, 20 330 símbolos, 472 globales (uno por programa: `_start`).

**Reparto (`elf_layout`).**

| | |
|---|---|
| archivo | cabecera ELF (64 bytes), 1 o 2 cabeceras de programa (56 bytes cada una), relleno de ceros hasta `0x1000`, `.text`, `.rodata` si el programa la tiene (alineada a la suya), relleno hasta la alineación de `.data`, `.data` |
| segmento 1 | `PT_LOAD`, R+X (`p_flags` 5), `p_offset` 0, `p_vaddr` `0x400000`, `filesz == memsz`, `p_align` `0x1000`: cabeceras, `.text` (`0x401000`, offset `0x1000`) y `.rodata` |
| segmento 2 | `PT_LOAD`, R+W (6), solo si hay `.data` o `.bss`: `p_vaddr = (fin del segmento 1, subido a página) + (p_offset mod 0x1000)`, de modo que `vaddr ≡ offset (mod 0x1000)`; `.data` al inicio, `.bss` después, alineada a la suya; `p_filesz` = tamaño de `.data`, `p_memsz` llega al fin de `.bss` |
| entrada | `e_entry` = dirección de `_start` (debe estar definida en `.text` y declarada `global`; si no, error) |
| sin | `PT_GNU_STACK`, `PT_NOTE`, `PT_DYNAMIC`. (Cabeceras de sección: M4 no las escribía; desde M5 van al final del archivo, ver [M5](#m5-symtab-strtab-shstrtab-y-comparación-de-text-con-ld).) |

Los bytes que mapean los segmentos terminan en el último byte de `.data` (o en el del segmento 1 si no hay datos; `ElfLayout.load_end`); desde M5 el archivo sigue con las tablas de símbolos y de secciones. `.bss` solo existe en memoria. Las alineaciones de sección que pide el programa (`align 32` en `.text` o `.data`, hasta 4096) se cumplen porque los inicios de cada segmento son múltiplos de página y el relleno se calcula con la alineación de cada sección; mayor que 4096 se rechaza.

**Relocaciones (`elf_patch`).** No se guarda ninguna: con las direcciones de las secciones ya fijas se escribe `S + A` (`R_X86_64_64`, 8 bytes; `R_X86_64_32`, 4 bytes sin signo; `R_X86_64_32S`, 4 bytes con signo) o `S + A − P` (`R_X86_64_PC32`), con `S` la dirección de la sección destino y `A` el addendo que `asm.c` ya trae (incluye el offset de la etiqueta). Un valor que no cabe en el campo es un error, no un truncamiento. `movabs` se conserva (`mov rax, etiqueta` sigue siendo de 10 bytes con el campo de 8 bytes parcheado): el tamaño del código no cambia respecto de nasm y `.text` sigue siendo idéntico, byte a byte, salvo los campos relocados.

**Decisiones.**

1. *Rechazar, no caer en silencio.* `--native` con `-S`, `-c`, `--freestanding`, `--flat`, `--boot` o `--elf`, y un programa con `asm("...")` o con una función `naked`, `interrupt` o `syscall` (el compilador solo permite `bits 16`/`bits 32` en funciones `naked`), termina con un error que dice qué (y la línea) y que sin `--native` se usa nasm + ld. No se escogió el respaldo automático porque quien pide `--native` está probando el camino nativo: que un programa «funcione» por haber usado nasm escondería justamente lo que se quiere saber, y en el CI una caída silenciosa daría una pasada verde que no probó nada. Se detecta en el generador (`native_note`), no en el ensamblador: el texto de `asm("...")` podría pasar por `asm.c` si usa instrucciones que cubre, y aun así no se le ha pedido a nadie que verifique ese ensamblador de usuario. Lo que el generador no marca pero `asm.c` no acepta (una directiva o instrucción fuera del inventario) también es un error, con la línea del texto NASM, nunca un subconjunto.
2. *`.rodata` en el segmento 1.* La tabla de 2.6 solo nombra `.data` y `.bss`; el generador hosted no usa `.rodata` (0 bytes en el corpus), así que esto solo lo ejercitan los casos propios. Se la deja con los permisos de lectura y no de escritura (como hace `ld`), sin añadir un tercer `PT_LOAD`.
3. *Sin `PT_GNU_STACK`.* El archivo no declara si quiere pila ejecutable. Un `.o` de nasm sin la nota `.note.GNU-stack` produce lo mismo en `ld` (que, según se recuerda de binutils y no se comprobó aquí, lo trata como «pide pila ejecutable»); si Linux exige otra cosa, el síntoma es un fallo al arrancar y la corrección es añadir la cabecera, ver «Lo que no se verificó».
4. *Un solo `gcc` sigue construyendo el compilador.* `copihue.c` incluye `compiler/native/{x64,x64_line,asm,elf}.c` (con `#define` temporales para los nombres `emit`, `parse_expr` y `trim`, que el compilador y los fuentes nativos usan para cosas distintas). Los tests de M1–M3 siguen compilando esos archivos por separado y no cambian. El `Makefile` hace depender `copihue` de los fuentes nativos.
5. *El texto sigue pasando por `output.asm`.* Es lo que da el oráculo (`-S`) y evita tocar el generador de 21 000 líneas (2.1, opción 1). Cambiarlo por un búfer en memoria es un cambio mecánico y se hizo en M6.

**Prueba.** `tests/x64/run_elf.sh` (parte de `make native-test`; sola, `make native-elf-test`) con `tests/x64/test_elf.c`. No ejecuta nada. Con un lector ELF propio y el `.o` de nasm del mismo texto como oráculo comprueba, por programa: cada campo de la cabecera ELF y de cada cabecera de programa; que haya una cabecera de programa si y solo si hay datos o `.bss`; `vaddr ≡ offset (mod 0x1000)`, `p_align`, `filesz ≤ memsz`, segmentos en páginas distintas, tamaño exacto del archivo, relleno de ceros; `e_entry` = `0x401000 + valor de _start` en el objeto de nasm; las direcciones de `.rodata`, `.data` y `.bss`, **leídas de las cabeceras de programa y no del cálculo del escritor**, alineadas como pide el programa y sin huecos mayores que la alineación; y que los bytes de `.text`, `.rodata` y `.data` del archivo sean los de nasm con **las relocaciones de nasm** (su propia `.rela`) aplicadas a esas direcciones. Entradas: 9 programas de `tests/x64/elf_cases.asm` (código que cruza páginas; solo `.data`; solo `.bss`; `.bss` alineado a 64 tras 5 bytes de datos; `align 32` y punteros a `.text` en datos; `.rodata` con `align 64`; las cuatro secciones con `align 4096`; datos en el borde de una página), 5 rechazos (sin `_start`, `_start` no `global`, `_start` fuera de `.text`, sin código, relocación que no cabe), los 472 programas del corpus (`-O0` y `-O2`), y 8 ejecutables escritos por `copihue --native` (4 programas × 2 niveles), leídos del disco: además de la estructura deben ser **idénticos** a lo que arma la biblioteca en memoria y tener modo `0755`. `tests/run_tests.sh --native` (`make native-run-test`) compila con `--native` todos los programas de la pasada normal y los **ejecuta** con las mismas salidas doradas en `-O0` y `-O2`; solo corre en Linux (en otro sistema se niega con un mensaje). `tests/lang/cpuid_carry.lap` y `tests/lang/naked_param.lap` usan `asm()`/`naked`: `--native` los rechaza, y esa pasada los cuenta como omitidos (con su nombre) solo si el fuente contiene una de esas construcciones. La sección 9 de `tests/run_tests.sh` (en la pasada normal, sin ejecutar nada) comprueba los rechazos: `tests/native/refuse_*.lap` y las combinaciones con `-S`, `-c`, `--freestanding`, `--flat` y `--boot`, con su mensaje y sin dejar ningún archivo. El CI ejecuta `make native-run-test` y `make native-test`.

**Resultado (nasm 3.02, macOS arm64).**

| | |
|---|---|
| símbolos | 472 de 472 programas con la tabla de símbolos igual al `.symtab` de nasm (20 330 símbolos) |
| estructura y contenido | 9 de 9 casos propios, 472 de 472 del corpus y 8 de 8 ejecutables escritos por `copihue --native`: sin diferencias |
| tamaño | corpus: 2 626 096 bytes de ELF en total (programa típico de 4 a 7 KiB); 454 de los 472 con segmento de datos; 7 296 relocaciones parcheadas |
| rechazos | 5 de 5 del escritor; 8 de 8 en la sección 9 |
| `objdump` | solo como comprobación ajena: el `objdump` de macOS (LLVM) lee dos de estos archivos (`hello`, sin datos, y `test_global_string_use`, con datos) y muestra las cabeceras de programa esperadas: `LOAD off 0 vaddr 0x400000 … r-x` y, el segundo, `LOAD off 0x1130 vaddr 0x402130 … rw-` |

**Lo que no se verificó.**

1. **Ningún ejecutable producido se ha ejecutado.** Esta máquina es macOS: no corre ELF de Linux, y tampoco hay `ld` de ELF ni `readelf`. Todo lo anterior son comprobaciones de estructura y de bytes. Que el núcleo de Linux cargue el archivo, que `_start` arranque y que cada programa imprima lo mismo que con nasm + ld solo lo dirá `make native-run-test` en el CI.
2. **No se comparó con lo que produce `ld`.** El reparto de segmentos es el de 2.6 (más `.rodata` en el segmento 1), no una copia del de `ld`. La comparación de `.text` con `objcopy -O binary -j .text` de un ejecutable de `ld` es M5. Direcciones de las mediciones del CHANGELOG (p. ej. `fib` en `0x4010df`): `.text` está en `0x401000` en este escritor, igual que lo supone el CHANGELOG para `ld`, pero eso tampoco se comprobó contra un ejecutable de `ld`.
3. **Ausencia de `PT_GNU_STACK`** (decisión 3): si el núcleo o la política de seguridad del CI tratan distinto un binario sin esa cabecera, el síntoma será un fallo al arrancar o un aviso; no se pudo probar.
4. **nasm 2.16.01** (el del CI) no está en esta máquina: las comparaciones de símbolos, relocaciones y bytes son contra nasm 3.02. El resto de M2/M3 ya lo decía.
5. **`gcc -Wall -Werror`.** Esta máquina compila con clang (`gcc` es un alias): al incluir los fuentes nativos en `copihue.c`, el paso «warnings are errors» del CI los compila por primera vez con `-Werror` y con el gcc de Ubuntu; no hubo advertencias con clang `-Wall -Wextra`, pero gcc puede avisar de otras cosas.
6. **`tests/run_tests.sh --native`**: su lógica de omitir los dos programas que `--native` rechaza y de fallar si rechaza otro se probó en macOS con un `uname` y un `timeout` simulados: los dos omitidos salen nombrados y el resto falla con `exit=126` porque macOS no ejecuta ELF; que todo lo demás pase es cosa del CI.
7. **`make native-test` en esta máquina** termina con `x64: FAIL` por una diferencia que no es de M4: el modo `vectors`/`edge`/`cases`/`corpus` de M1 imprime `DIFF movsxd rax, eax` porque nasm 3.02 acorta ese `movsxd` a `cdqe` (el codificador sigue a nasm 2.x, el del CI; ya está en el CHANGELOG). Las partes de M2, M3 y M4 salen `OK`. No se comprobó contra `main` en un árbol aparte (no se pudo crear aquí); los archivos de M1 no se tocaron.
8. Los límites de la biblioteca (1 GiB por sección, alineación de sección hasta 4096) son elecciones propias, como en M3. No se midió el tiempo ni el tamaño frente al camino de nasm + ld.

### M5. `.symtab`, `.strtab`, `.shstrtab` y comparación de `.text` con `ld`

**Qué se agrega a `elf.c`.** Después de los bytes que mapean los segmentos (`load_end`), alineados a 8 y en este orden: `.symtab`, `.strtab`, `.shstrtab` y la tabla de cabeceras de sección. Nada de lo anterior se mueve: los segmentos, las direcciones y los bytes cargados son los de M4; el archivo solo crece (`tests/hello.lap` en `-O0`: 4 339 → 4 920 bytes). `e_shoff`, `e_shnum` y `e_shstrndx` quedan puestos; `e_shentsize` es 64.

| | |
|---|---|
| secciones | nula, `.text`, `.rodata`, `.data`, `.bss` (las que tengan bytes), `.symtab`, `.strtab`, `.shstrtab`. `.text` es `PROGBITS` `AX`; `.rodata`, `A`; `.data`, `WA`; `.bss`, `NOBITS` `WA` con `sh_offset` = fin de los bytes cargados. `sh_addr`, `sh_offset`, `sh_size` y `sh_addralign` salen del reparto y de la alineación que ya calculó `asm.c` |
| `.symtab` | símbolo nulo, luego los `LOCAL`, luego los `GLOBAL`; `sh_info` = índice del primer `GLOBAL`, `sh_link` = `.strtab`, `sh_entsize` 24 |
| qué símbolos | toda etiqueta del programa **sin punto** en el nombre y en una sección con bytes. En `.text`, `FUNC`; en `.data`/`.rodata`/`.bss`, `OBJECT`. `GLOBAL` si una línea `global` la nombra (el compilador solo emite `_start`), `LOCAL` si no. No se emiten las subetiquetas `padre.hijo` (`.loop`, `.L12`): son destinos de salto, no funciones ni datos |
| valor y tamaño | `st_value` es la dirección final. `st_size` llega hasta la siguiente etiqueta de la misma sección con offset mayor, o hasta el fin de la sección: **incluye** el relleno de `align` y el código de las subetiquetas. nasm deja todos los tamaños en 0; este es un criterio propio |
| sin | `.rela.*` (no queda ninguna relocación), `.note.*`, `STT_FILE`/`STT_SECTION`, `_end`/`_edata`/`__bss_start` (los que añade `ld`) |

**Prueba de estructura (sin Linux).** `tests/x64/test_elf.c` (`verify_tail`), dentro de `make native-elf-test` y `make native-test`: lee las cabeceras de sección y la tabla de símbolos con un lector propio y las compara con el objeto de `nasm -f elf64` del mismo texto: nombres, `e_shnum`/`e_shstrndx`, las tablas contiguas al final del archivo y rellenos en cero, `sh_addr`/`sh_offset`/`sh_size`/`sh_flags`/`sh_addralign` de cada sección contra los programas de cabecera, y cada símbolo (nombre, enlace, tipo, índice de sección, dirección, tamaño); el número de símbolos debe ser el de etiquetas sin punto que tiene nasm. Resultado con nasm 3.02: 472 de 472 programas del corpus, 10 de 10 casos propios (`elf_cases.asm`, uno nuevo con `global` de función y de dato, subetiquetas, relleno de `align` y `.bss` con etiquetas) y los `--native` reales, 0 fallos. Antes `e_shoff` debía valer 0; esa comprobación se quitó.

**Lo que se vio con herramientas ajenas (a mano, en macOS).** El `objdump` de LLVM que trae macOS lee el archivo: `objdump -h -t` lista `.text`, `.data`, `.symtab`, `.strtab`, `.shstrtab` y los símbolos (`_start` como `g F .text`, `main` como `l F .text`, `msg` como `l O .data`), y `objdump -d` desensambla con `<_start>:` y `<main>:` y los saltos con `<__strcmp+0x1d>`. Es una comprobación a ojo sobre `tests/test_global_string_use.lap`, no una prueba automática. No hay `readelf` en esa máquina.

**Comparación de `.text` con `nasm` + `ld` (`tests/x64/run_text.sh`, `make native-text-test`).** Para cada programa de `tests/` y `examples/` (los mismos que `run_tests.sh`) en `-O0` y `-O2`: se construye con `--native` y con `nasm -f elf64` + `ld`, se extrae `.text` con `objcopy -O binary -j .text` de ambos y se compara con `cmp`. Tres resultados: *idéntico*; *layout* (distinto, pero idéntico al volver a enlazar el objeto con `ld --section-start` poniendo `.data`/`.bss`/`.rodata` en las direcciones del escritor nativo); *DIFF* (distinto aun así: falla). Con `nm`, además, cada símbolo del archivo nativo debe estar en el de `ld` con la misma dirección. Si falta `nasm`, `ld` GNU para x86-64, `objcopy` u `objdump`, el script lo dice y termina con 0; con `CMP_REQUIRE=1` (lo pone el CI) termina con error. `tests/x64/run_readelf.sh` (`make native-readelf-test`) hace que `readelf -hSsW` y `objdump -d` lean tres programas en ambos niveles y busca avisos, las secciones, `_start` `GLOBAL FUNC`, `main` `LOCAL FUNC` y un `OBJECT` cuando hay `.data`.

**Qué casos se espera que difieran, y por qué (una predicción, no una medición).** El código sin referencias a datos no depende de dónde se ponga el segmento de datos: su `.text` debería ser idéntico. El de los programas que sí las tienen (la mayoría: 454 de 472 tienen segmento de datos) lleva direcciones absolutas de `.data`/`.bss` dentro de `mov`/`lea`; el escritor nativo pone `.data` en la página siguiente al fin de `.text` más el desplazamiento dentro de página, y `ld` con su guion por defecto puede dejarla en otra (`-z separate-code` añade un alineado a página antes de los datos). Si es así, esos programas saldrán como *layout* y no como *idéntico*, y la diferencia son solo esos campos de 4 u 8 bytes. M2 y M3 ya mostraron que los bytes y las relocaciones coinciden con los del objeto de nasm, así que un *DIFF* indicaría un error nuevo, p. ej. en cómo el escritor aplica una relocación. Qué sale de verdad lo dirá la primera ejecución en el CI; **hasta entonces no se sabe si algún caso difiere ni por qué**, y esta sección no debe leerse como un resultado.

**Lo que no se verificó en M5.**

1. **`run_text.sh` y `run_readelf.sh` no se han ejecutado con herramientas reales.** Esta máquina no tiene `objcopy`, `readelf` ni un `ld` que enlace ELF. Se probó la lógica del script con herramientas falsas (`objcopy` y `ld` de sustitución en `PATH`): la omisión por falta de herramientas (0, y 1 con `CMP_REQUIRE=1`), el recorrido de programas, el salto de los que `--native` rechaza, la ruta «idéntico», la ruta *DIFF* con su salida y el fallo por símbolos. No se probaron el análisis de `objdump -h` (el formato se comprobó con el de LLVM, donde la columna 4 también es la dirección), el filtro de `nm` ni `ld --section-start` con un `ld` de verdad.
2. **Los tamaños de símbolo** (hasta la siguiente etiqueta) son una decisión propia; `readelf -s` los mostrará y `objdump -d` no los usa para nada.
3. **Ejecución**: sigue sin ejecutarse ningún binario; ver M4.
4. **Etiquetas en secciones vacías** (una etiqueta en una sección sin bytes) no se emiten; el compilador no produce ese caso, pero `ld` sí las dejaría. El conteo de símbolos de `ld` sin contraparte nativa que imprime el script lo mostraría.

### M6. El camino nativo por defecto y los objetos `-c`

**Qué cambió.** Sin opciones, `copihue programa.lap -o salida` genera el texto NASM en memoria, lo ensambla con `asm.c`, escribe el ELF con `elf.c` y no llama a `nasm` ni a `ld`; tampoco deja `output.asm` ni `output.o`. `codegen` escribe en un búfer (`open_memstream`) cuando no recibe una ruta; `-S`, `--flat`, `--boot` y `--nasm` siguen pasando por el archivo, así que `-S` sigue siendo el oráculo contra el que se comparó todo.

| invocación | qué hace |
|---|---|
| (sin opción) | ruta nativa; si el programa trae algo que el ensamblador propio no toma, cae a `nasm` + `ld` con un aviso |
| `--nasm` | siempre `output.asm`, `nasm -f elf64` y `ld`; deja `output.asm` y `output.o` (el camino de antes de M4) |
| `--native` | la ruta por defecto, estricta: rechaza (mismos mensajes de M4) en lugar de caer; ahora también con `-c`; con `-S`, `--freestanding`, `--flat`, `--boot` y `--elf` sigue siendo error; no hace falta cambiar los scripts que lo pasan |
| `--native --nasm` | error |
| `-c` | objeto `ET_REL` nativo; con `--freestanding` lo hace `nasm`, sin aviso |
| `-S`, `--flat`, `--boot`, `--freestanding` sin `-c` | sin cambios |

**Decisión: caer a `nasm` con aviso, no soportar ni rechazar.** Los programas con `asm("...")` y las funciones `naked`/`interrupt`/`syscall` (y las de `bits 16/32`, que son `naked`) pueden llevar cualquier instrucción, modos de 16 y 32 bits, registros de control; soportarlos es inventariar y codificar todo x86, que es otro trabajo y no se hizo. Rechazarlos por defecto rompería programas que hoy compilan. Así que caen al camino viejo y lo dicen: `copihue: warning: the built-in assembler does not take this program (line 5: asm("...") (user assembly)); falling back to nasm and ld` (con `-c`, `falling back to nasm`). Lo mismo si `asm.c` rechaza el texto generado (el aviso lleva su mensaje). Costos: el aviso sale en cada compilación de esos programas, y esos programas siguen necesitando `nasm` y `ld`. Para que la caída no esconda huecos, `tests/run_tests.sh` falla si un programa **sin** esas construcciones cae a `nasm` (antes lo hacía `--native` rechazando), y `--native` queda como la forma estricta para quien la quiera.

**Objetos `-c` (`elf_build_obj`).** `ET_REL`, sin cabeceras de programa; secciones `.text`, `.rodata`, `.data`, `.bss` (las que tengan bytes), un `.rela.text`/`.rela.rodata`/`.rela.data` por cada una con relocaciones, `.symtab`, `.strtab`, `.shstrtab`, cada sección alineada a su `sh_addralign`. `.symtab`: símbolo nulo, un símbolo `SECTION` por sección, los `LOCAL` sin punto, luego los `GLOBAL` y los `extern` indefinidos (`sh_info` = el primero no local). Para esto `asm.c` aprendió `extern` (solo en `asm_assemble_obj`; el ejecutable lo sigue rechazando con su mensaje de M3): un `call`, `jmp` o `jcc` a un extern es la forma larga con relocación; un operando que lo nombra, o un `dq`/`dd`, deja la relocación contra el nombre (`AsmReloc.sec = ASM_SEC_UNDEF`).

Medido en el texto que emite el compilador con `-c` (nasm 3.02): `call ext` → `R_X86_64_PC32` con addendo −4 (no `PLT32`); `[ext]` y `[ext+8]` → `R_X86_64_32S`; `mov rax, ext` y `dq ext` → `R_X86_64_64`; `dd ext` → `R_X86_64_32`; `[rel ext]` → `PC32` con −4; una etiqueta del programa, `global` o no, → el símbolo de su sección con el offset en el addendo (M3); un `call` a otra función del mismo objeto se resuelve sin relocación. nasm **omite** un `extern` que nada usa y un `global` que nada define; el objeto nativo también. Diferencias deliberadas con el objeto de nasm: no hay `STT_FILE`, ni símbolos de las constantes `equ` (`__frame_f`) ni de las subetiquetas (`.loop`, `.L12`), y los símbolos llevan tipo (`FUNC`/`OBJECT`) y tamaño (hasta la etiqueta siguiente, como en M5) donde nasm deja `NOTYPE` y 0. Ninguno escribe `.note.GNU-stack`: un `ld` reciente puede avisar de pila ejecutable, como con el objeto de nasm.

**Pruebas.**

- `tests/x64/test_obj.c` (`tests/x64/run_obj.sh`, `make native-obj-test`; también al final de `make native-test`). Para cada texto: `nasm -f elf64` y `asm_assemble_obj` + `elf_build_obj`; compara con un lector propio (`nobj.h`) los bytes de `.text`, `.data`, `.rodata`, el tamaño de `.bss`, `sh_addralign`, cada relocación (offset, tipo, addendo, símbolo o símbolo de sección) y cada símbolo (sección, valor, enlace, tipo, tamaño, y el conjunto de indefinidos). Aparte revisa la estructura del archivo solo: `ET_REL`, el campo `e_*`, que ninguna sección se solape ni caiga en la tabla de cabeceras, la alineación, `sh_link`/`sh_info`/`sh_entsize` de cada `.rela`, el orden de los símbolos, que cada relocación nombre un símbolo válido y que la tabla de cabeceras termine el archivo. En `obj`, el archivo que escribió `copihue -c` debe ser idéntico a lo que construye la biblioteca. Entradas: 9 programas propios (`obj_cases.asm`: llamadas, `jmp` y `jcc` a externs, todas las formas de dirección de un extern, externs en `.data`/`.rodata`/`.text`, etiquetas de cada sección usadas desde cada sección, solo datos, solo `.bss`, locales, externs sin uso, `align` y tamaños), 8 rechazos, el texto de `-c -S` de todo `tests/` y `examples/` en `-O0` y `-O2` más `tests/link/parts` (498 archivos), y 12 `copihue -c` reales leídos del disco.
- En Linux, con `gcc`, `ld`, `readelf` y `objdump`, `run_obj.sh` además enlaza `tests/link/c_main.c` (un `main` de C con libc que llama a `add` y lee `bias` de un objeto nativo; espera `42 7`), enlaza con `ld` las cuatro combinaciones de objeto nativo y objeto de nasm de `mod_add` y `mod_main` (espera `42`) y deja que `readelf -hSsrW` y `objdump -dr` lean los objetos sin avisos. Fuera de Linux, o sin una de esas herramientas, lo dice y sigue; con `CMP_REQUIRE=1` (lo pone el CI) es un error.
- `tests/run_tests.sh`: la pasada por defecto es la nativa (falla si algo cae a `nasm` sin `asm()`/`naked`/`interrupt`/`syscall`); `--nasm` corre los mismos programas con el camino viejo (`make nasm-test`); `--native` queda como la estricta (`make native-run-test`). Sección 9b: con `nasm` y `ld` sustituidos por programas que registran la llamada y fallan, el build por defecto y `-c` no los llaman, no dejan `output.asm` ni `output.o`, no imprimen nada, y escriben `ET_EXEC` y `ET_REL`; el aviso de caída sale con su línea para `asm()` y `naked`, en ejecutable y en `-c`; `--freestanding -c` no avisa. La sección 8 (enlazar objetos `-c` con `ld`) ahora usa objetos nativos. Fuera de Linux (`uname`), compila todos los programas pero no los ejecuta, y salta con aviso las partes que enlazan con `ld`.
- CI: `make test` (nativo), `make nasm-test`, `CMP_REQUIRE=1 make native-test`, y los pasos de M4/M5 que ya había (`native-readelf-test`, `native-text-test`). `make native-run-test` ya no está en el CI porque la pasada por defecto hace lo mismo.

**Resultado en esta máquina (macOS arm64, nasm 3.02, `clang` como `gcc`).**

| | |
|---|---|
| objetos `-c` contra nasm | 9 de 9 casos propios; 498 de 498 archivos del corpus; 12 de 12 `copihue -c` leídos del disco; 8 de 8 rechazos. 7 332 relocaciones, 9 229 símbolos definidos y 16 externs comparados (bytes de `.text`/`.data`/`.rodata`, tamaño de `.bss` y alineaciones incluidos) |
| `objdump -h -t -r` de LLVM | lee `tests/link/parts/mod_main.lap` como objeto: `.text`, `.data`, `.rela.text`, `.symtab`; `_start` y las otras funciones `g F .text`, `add` y `bias` `*UND*`, `R_X86_64_32S bias`, `R_X86_64_PC32 add-0x4`, `R_X86_64_64 .data` (a mano; no hay `readelf`) |
| tamaño de ese objeto | 1 328 bytes el nativo, 1 952 el de nasm (el de nasm lleva 13 símbolos más: `STT_FILE`, las 11 subetiquetas y `__frame_main`); no se midió nada más |
| programas por la ruta nativa por defecto | 228 de 228 compilan sin caer a `nasm` (`-O0` y `-O2`); caen, como se espera, `tests/lang/cpuid_carry.lap` y `tests/lang/naked_param.lap` |
| `make native-test` (`tests/x64/run.sh`) | todas las partes de M2–M6 en `OK`; la parte de M1 sigue con `DIFF movsxd rax, eax` (nasm 3.02 lo acorta a `cdqe`, el codificador sigue a 2.x) |
| `tests/run_tests.sh` | 251 pruebas pasan; **45 fallan en esta máquina, todos en `tests/flat`, `tests/boot`, `tests/usermode` y `tests/ctx`**, que no pasan por la ruta nativa (usan `--flat`/`--boot` y `nasm -f bin`) y que no se tocaron: sus pruebas comparan volcados `od -An -tx1` contra los guardados y el `od` de macOS pone un ancho de columna distinto (los bytes de `tests/flat/entry.lap` son idénticos a su archivo guardado: se comprobó a mano con un caso), y QEMU no arranca los discos aquí (no se crea `serial.txt`). No se comprobó contra `main` que ya fallaran antes del cambio. Se saltan con aviso los dos programas que caen a `nasm`, las pruebas que enlazan con `ld` (sección 8 y el enlace freestanding) y la comprobación de `--nasm` |

**Lo que no se verificó.**

1. **Nada de esto se ha ejecutado ni enlazado.** El enlace con `ld` y `gcc`, la ejecución de los programas y de `tests/link/c_main.c`, y `readelf` quedan para el CI. Lo comprobado aquí es igualdad de bytes, relocaciones y símbolos contra nasm, estructura del archivo y lectura con el `objdump` de LLVM.
2. **nasm 2.16.01** (el del CI) no está en esta máquina. Las relocaciones de `call` a un extern (`PC32`, no `PLT32`), la omisión de externs y globals sin uso, y que no haya `.note.GNU-stack` están medidas con nasm 3.02; si 2.16 difiere, `test_obj` nombra el programa y la relocación.
3. **`gcc` 14 con `-Wall -Werror`** no está aquí (`gcc` es `clang`): el código nuevo compila sin advertencias con clang (`-Wall -Werror` para el compilador, `-Wall -Wextra` para las pruebas), no con gcc.
4. **«Las mediciones del CHANGELOG sin cambios»** (criterio de aceptación de M6) no se comprobó: no se corrió `make bench` ni se midió tiempo de compilación, tamaño ni velocidad. Que `.text` sea el mismo que el de nasm + ld lo dice `make native-text-test` en el CI, no esta sesión. Tampoco se afirma que compilar sea más rápido o más lento que antes.
5. Un programa con `asm("...")` que `asm.c` aceptaría (por ejemplo `nop`) igualmente cae a `nasm`: no se le pidió a nadie que verifique el ensamblador de usuario (M4, decisión 1).
6. Los `extern` de datos que codegen emite en `-c` y que el programa solo usa en un `dq` o `dd` de `.data` se comparan con nasm en los casos propios (`obj_cases.asm`), no en el corpus (ningún programa lo hace).
7. Sin `STT_FILE`, `gdb` no muestra el nombre del archivo fuente de un objeto nativo; no se probó con ningún depurador.


Medida con `make native-test` (`tests/x64`) sobre el corpus de 466 archivos; «cubierta» significa que `x64_encode` produce bytes y que esos bytes (y la relocación, si la hay) **coinciden con nasm**.

| | ocurrencias cubiertas | % |
|---|---|---|
| todas | 181 987 de 181 987 | **100 %** |
| instrucciones distintas (símbolos renombrados) | 4 334 de 4 334 | 100 % |
| archivos que se ensamblarían enteros | 0 de 466 (solo con `x64.c`; con M2, `asm.c`: 466 de 466 en `.text`; con M3, 466 de 466 enteros: `.text`, `.data`, `.bss` y relocaciones) | 0 % (solo `x64.c`) |

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
