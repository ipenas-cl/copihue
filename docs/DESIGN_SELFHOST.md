# Diseño: autoalojamiento (Copihue se compila a sí mismo)

**Estado: aprobado el 2026-11-05 por Ignacio Peña. S0 entregada; S1 entregada el 2026-11-10 en Copihue 0.45.0, pendiente de su revisión.** Todavía no hay código de autoalojamiento en este árbol: S1 cambia el lenguaje, no reescribe el compilador. La aprobación cubre el plan; cada fase siguiente arranca solo con su criterio de aceptación anterior en verde. El inventario, el corpus y la lista de huecos de S0 están en [SELFHOST_S0.md](SELFHOST_S0.md); lo entregado en S1, en [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md).

Corte de referencia: Copihue **0.44.8** (`compiler/bootstrap-c/copihue.c`, `LANG_VERSION`; commit de `main` `c4a4036`). El paso 1 del camino nativo (escribir el ELF hosted sin `nasm` ni `ld`) está cerrado en [DESIGN_NATIVE.md](DESIGN_NATIVE.md) (M1–M6). Este documento es el **paso siguiente**: que el compilador deje de depender de C como implementación, y no solo de `nasm`/`ld` como herramientas externas.

Velocidad: solo con mediciones ([`bench/RESULTS.md`](../bench/RESULTS.md), [`examples/lowlat/RESULTS.md`](../examples/lowlat/RESULTS.md)). Este plan no afirma tiempos ni tamaños nuevos. Los dos niveles de sintaxis (fácil / explícito, [DESIGN_TWO_LEVELS.md](DESIGN_TWO_LEVELS.md)) son meta de lenguaje aparte; aquí solo importan si un hueco de ese diseño bloquea expresar el compilador.

## 1. Meta

Copihue se compila a sí mismo y emite ELF **sin depender de C, de `nasm` ni de `ld`** en el camino hosted que ya cubre `compiler/native`.

### Definición de hecho

Hay autoalojamiento cuando, en un corpus acordado de programas `.lap` (como mínimo el conjunto hosted que hoy pasa `tests/run_tests.sh` por la ruta nativa, más el propio fuente del compilador en Copihue):

1. Existe un `copihue` **escrito en Copihue** (árbol bajo algo como `compiler/self/` o el layout que fije la Fase 0).
2. Ese `copihue` se construye primero con el bootstrap C (o con un `copihue` ya autoalojado anterior): el C sigue siendo el **arranque**, no la implementación de producción a largo plazo.
3. El ELF hosted que produce el `copihue` en Copihue, al compilar cada programa del corpus con las mismas opciones (`-O0` / `-O2`, sin `--nasm`), es **idéntico byte a byte** al que produce el bootstrap C de la misma versión (o, si se acuerda un oráculo más débil en una fase intermedia, idéntico en `.text` / secciones y misma salida al ejecutar; el criterio fuerte es el byte a byte del ejecutable o del objeto `-c`).
4. En ese camino no se invoca `nasm` ni `ld`. El ensamblador y el escritor ELF son los de `compiler/native` (hoy en C; luego portados o reutilizados desde Copihue según las fases).

Hasta que (1)–(4) estén verdes en CI Linux, **no** se borra ni se deja de construir `compiler/bootstrap-c/copihue.c`.

## 2. Estado actual (0.44.8)

Hechos verificables en el árbol y en la documentación vigente; no se inventan cifras de corridas nuevas aquí.

| Pieza | Dónde | Estado |
|---|---|---|
| Bootstrap del compilador | `compiler/bootstrap-c/copihue.c` (un solo archivo C; 23.590 líneas según `wc -l` en 0.44.8; mapa por bloques en [SELFHOST_S0.md](SELFHOST_S0.md) §1) | Es la implementación. `make` construye `./copihue` con `gcc`. |
| Camino nativo hosted | `compiler/native/` (`x64`, `asm`, `elf`) | M1–M6 hechos ([DESIGN_NATIVE.md](DESIGN_NATIVE.md)): por defecto el compilador arma el texto NASM en memoria, lo ensambla y escribe el ELF (`ET_EXEC` o `-c` `ET_REL`) **sin** `nasm` ni `ld`. `--nasm` queda el camino viejo. |
| Suite nativa | `make native-test`, `tests/run_tests.sh` por la ruta nativa | Criterios y coberturas en DESIGN_NATIVE (corpus hosted, igualdad con nasm en bytes/relocaciones/símbolos, pasada que ejecuta en Linux). |
| Lo que aún cae a `nasm`/`ld` | `asm("...")`, `naked` / `interrupt` / `syscall`, `bits 16`/`32`, `--freestanding -c`, `--flat`, `--boot` | Documentado en el README: el ensamblador nativo no los toma; con aviso caen al camino viejo, o `--native` estricto los rechaza. |
| Formatos de kernel | `--flat ADDR`, `--axe1`, `--axe2`, `--pic`, `--boot` | `--flat`, `--axe1`, `--axe2`, `--pic`, `--pe-efi`, `--sizes`, `align(N)` en `fn` y `let`, `ms64 fn` (0.44.1–0.44.8, CHANGELOG / README). Sirven a LapageriaOS; **no** son requisito del autoalojamiento hosted. |
| Lenguaje usable hoy | enums, `match`, `Result`/`?`, generics, traits, `str` como `[]u8`, `vec`/`own` locales, módulos, `-c`, atomics, hilos hosted | Cortes 0.36–0.44.x (incluye `const`, globales con inicializador constante, error al asignar a un no-lvalue) y notas `DESIGN_*.md`. Suficiente para programas grandes, **no** aún para expresar el compilador entero sin los huecos de la §3. |
| Autoalojamiento | — | **No empezado.** El README lo dice con claridad: *Self-hosting … are not started.* Material viejo bajo `docs/archive/` (era Chronos) **no** cuenta; no se reutiliza como estado. |

Relación con el paso 1: DESIGN_NATIVE cierra «emitir código máquina y ELF propios» para el hosted que el inventario cubre. La dependencia de C queda explícitamente fuera de ese documento («La dependencia de C … es otra meta y no entra aquí»). Esta propuesta es esa otra meta.

## 3. Huecos del lenguaje que bloquean expresar el compilador

Lista **honesta**, tomada de lo que ya marcan REFERENCE, STATUS, README y los `DESIGN_*.md`. No se inventan features ni se prometen APIs nuevas más allá de lo que el plan histórico ya nombró (memoria dinámica, tablas de símbolos, structs y `str` devolvibles, y lo mínimo que falte).

**Abiertos en S1 (0.45.0):** las dos primeras filas de la tabla de abajo ya no son huecos: un `str`/`[]T` se devuelve y es campo de struct ([DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md)). Antes de eso se arregló el retorno de un struct por valor, que devolvía una dirección del frame muerto.

**Cerrado en S0** ([SELFHOST_S0.md](SELFHOST_S0.md) §3, con sondas de compilación): S1 abre **dos** de estos huecos, devolver `str`/`[]T` y `str`/`[]T` como campo de struct. `vec T` como campo, parámetro, retorno o global **no** entra en S1: una tabla que crece se escribe con un `Vec<T>` genérico de la biblioteca del compilador (`struct Vec<T> { ptr: *T, len: i64, cap: i64 }`), que compila hoy con `T` entero o struct. El texto de salida y los mensajes usan un `Buf` de esa biblioteca, no `append` de `str`. La tabla de abajo es el punto de partida y se deja como estaba escrita.

### 3.1 Bloquean de frente (hay que cerrarlos antes o al empezar a portar)

| Hueco | Qué dice hoy el árbol | Por qué importa al compilador |
|---|---|---|
| No se puede **devolver** un `str` ni un `[]T` | `a slice cannot be returned` ([DESIGN_STRINGS.md](DESIGN_STRINGS.md), [DESIGN_SLICES.md](DESIGN_SLICES.md), `tests/errors/str_ret.lap`) | Lexer, parser y diagnostics viven de devolver trozos de texto y vistas; sin retorno de slice el estilo es puntero+largo a mano en cada firma. |
| Un `str` / `[]T` **no puede ser campo** de struct | `a slice cannot be a struct field` | Un token, un símbolo o un diagnóstico es un struct con texto; sin campo slice la tabla de símbolos no se modela limpio. |
| `vec T` no es parámetro, retorno, global ni campo | README / STATUS / DESIGN_TWO_LEVELS (cortes 0.36–0.37) | Tablas que crecen (símbolos, instrucciones, relocaciones) necesitan `vec` (o equivalente) **dentro** de structs y entre funciones, no solo como `let` local. |
| `str` no tiene heap ni `append`; es solo `[]u8` | DESIGN_STRINGS: sin UTF-8, sin `String`, sin formato, sin capacidad | Armar mensajes de error y buffers de salida exige `vec u8` / buffers manuales; hay que fijar en Fase 0 el idiom permitido. |
| Campo de `enum` no puede ser `str` ni slice | Solo entero o puntero ([DESIGN_ENUMS.md](DESIGN_ENUMS.md)) | Un AST con payloads de texto no entra en un enum; hay que usar punteros a structs o índices al buffer fuente (válido, pero hay que diseñarlo). |

Memoria dinámica **sí** existe en hosted: `malloc`/`free`, `vec_push`, `own *T` con move en retorno y parámetro. Eso cubre parte del «ampliar con memoria dinámica» del plan histórico; lo que falta es poder **colocar** esas estructuras en la forma en que un compilador las usa (campos, retornos, tablas).

### 3.2 No bloquean el primer autoalojamiento hosted (quedan fuera del criterio de hecho)

- Camino nativo para `asm("...")`, `naked`, `interrupt`, `syscall`, `--flat`, `--boot`, `--freestanding -c` (sigue DESIGN_NATIVE §2.7).
- Borrow checker / lifetimes (un `[]T` visto de un `vec` puede colgar; el compilador en Copihue se escribe con disciplina, igual que hoy en C).
- Generics sobre enums, slices, `vec`, `own *T`.
- Modo fácil completo de [DESIGN_TWO_LEVELS.md](DESIGN_TWO_LEVELS.md) (inferir `*T` por índice, etc.): el compilador autoalojado se escribe en estilo **explícito** (tipos anotados). No hace falta terminar los dos niveles para autoalojar.
- Otros targets (ARM64): fuera de alcance.

### 3.3 Plan histórico (solo el ya dicho; no se amplía)

Orden ya planteado y que esta propuesta respeta:

1. Emitir código máquina y ELF propios → **parcialmente hecho** (DESIGN_NATIVE M1–M6; hosted por defecto).
2. Ampliar el lenguaje (memoria dinámica, tablas de símbolos, structs y `str` devolvibles, lo que falte para expresar el compilador).
3. Reescribir el compilador **por partes** en Copihue, comparando **byte a byte**.
4. Dejar el C **solo como arranque**.

## 4. Fases

Cada fase es pequeña, tiene criterio de aceptación observable y no arranca la siguiente sin ese criterio en verde. Los nombres S0, S1, … evitan chocar con M1–M6 de DESIGN_NATIVE.

### S0 — Inventario y corpus (sin reescribir el compilador)

**Estado: entregada el 2026-11-05, pendiente de revisión.** Lo cerrado está en [SELFHOST_S0.md](SELFHOST_S0.md): inventario de bloques de `copihue.c` y orden de portado (§1), corpus de 236 programas × `-O0`/`-O2` con su lista en `tools/selfhost/corpus.txt` (§2), huecos de S1 y el idiom de tablas y mensajes (§3), y las cinco decisiones que quedan para Ignacio (§4).

**Qué.** Cerrar por escrito, en este mismo documento o en un anexo corto bajo `docs/`:

- Qué módulos del bootstrap C se portan y en qué orden (propuesta inicial: tablas / utilidades de buffer → lexer → diagnostics → parser/AST → semántica → generación de texto NASM → enlace con `compiler/native` ya existente).
- El **corpus de igualdad** byte a byte (lista de `.lap` + flags). Arranque: el hosted que hoy pasa por la ruta nativa en la suite; ampliar solo con acuerdo.
- La lista cerrada de huecos del §3.1 que S1 debe abrir, y el idiom para mensajes/buffers si `append` de `str` no entra.

**Aceptación.** Documento revisado; cero código nuevo de `compiler/self/`; la suite actual sigue verde sin cambios de comportamiento.

### S1 — Huecos mínimos del lenguaje

**Estado: entregada el 2026-11-10 (Copihue 0.45.0), pendiente de revisión.** Puntos 1 y 2, más el arreglo del retorno de struct; el punto 3 (`vec` en structs) quedó fuera por la decisión de S0 (`Vec<T>` en la biblioteca). ABI, vistas a memoria muerta, pruebas (incluida la ejecución en QEMU de los kernels de `tests/sret/`) y hallazgos: [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md).

**Qué.** Solo lo necesario para expresar tablas y texto en el estilo del compilador:

1. Devolver `str` / `[]T` (misma representación de 16 bytes; ABI acordada y documentada en DESIGN_STRINGS / REFERENCE).
2. `str` / `[]T` como campo de struct (y, si el diseño de S0 lo exige, como elemento usable en las tablas).
3. `vec T` como campo de struct y, como mínimo, poder pasarlo o exponerlo de forma que una tabla de símbolos viva en un struct (parámetro/retorno de `vec` si S0 lo marca imprescindible; si no, campo + puntero al struct basta).

Cada ítem trae tests de lenguaje y de error (el estilo de `tests/lang/` y `tests/errors/`). No se reescribe el compilador en esta fase.

**Aceptación.** Suite verde; REFERENCE y el DESIGN correspondiente actualizados; ningún cambio de semantics en programas que no usen lo nuevo (`-O0` y `-O2` iguales en la suite, como siempre).

### S2 — Primer módulo en Copihue (prueba del método)

**Qué.** Un solo trozo del compilador escrito en Copihue (el candidato natural tras S0: lexer, o el buffer/tabla de símbolos si el lexer aún no cabe). Se construye con el bootstrap C. Para un conjunto chico de entradas, la salida del módulo (tokens, tabla, o el artefacto que S0 fije) coincide con la del bootstrap C, o se compara el ELF de un driver de prueba byte a byte.

**Aceptación.** Script o target de `make` (sin Python) que falle si hay divergencia; CI Linux en verde; el resto del compilador sigue siendo C.

### S3 — Por partes hasta el generador

**Qué.** Sustituir, módulo a módulo, parser, AST, chequeos y generación del texto NASM (o, si S0 eligió alimentar `XInsn` en vez de texto —DESIGN_NATIVE §2.1 opción 2—, esa interfaz). Cada módulo nuevo se compara contra el C en el corpus parcial. El enlace al ELF sigue pudiendo ser el `compiler/native` en C al inicio de S3.

**Aceptación.** Por cada módulo fusionado: igualdad acordada (byte a byte del artefacto intermedio o del ELF de un driver) en el subcorpus; suite completa sin regresión.

### S4 — `native` en Copihue (opcional como hito aparte) y compilador completo

**Qué.** Cuando el front y el generador ya están en Copihue, portar o reexpresar `compiler/native` (asm/elf/x64) en Copihue **o** dejarlo enlazado como objeto C el tiempo que haga falta, documentando la excepción. El criterio de hecho de la §1 no exige que `native` esté en Copihue el mismo día que el front; sí exige que el usuario final no necesite `nasm`/`ld` ni un compilador C para **usar** el `copihue` autoalojado en el camino hosted. Construir el autoalojado desde cero en una máquina limpia **sí** puede seguir pidiendo `gcc` una vez (arranque).

**Aceptación (definición de hecho).** El `copihue` en Copihue produce el mismo ELF (o el oráculo fuerte que S0 fijó) que el bootstrap C en **todo** el corpus acordado; CI verde; README actualizado: autoalojamiento alcanzado, C = arranque.

### S5 — C solo como arranque

**Qué.** El camino documentado de desarrollo y release usa el binario autoalojado. `compiler/bootstrap-c/copihue.c` permanece en el árbol para bootstrap y emergencias. No se elimina en S5.

**Aceptación.** Instrucciones de bootstrap en README; tag/release que construye con el ciclo autoalojado en CI; C sin borrarse.

## 5. Qué NO se hace en esta propuesta

- No reescribir todo el compilador de un golpe.
- No borrar ni dejar de compilar el C hasta que la definición de hecho (§1) esté verde en CI.
- No implementar código de autoalojamiento **fuera del orden de fases: nada de `compiler/self/` antes de cerrar S0.
- No mezclar en el mismo hito el autoalojamiento hosted con portar `--flat` / `--boot` / `naked` al ensamblador nativo.
- No exigir el modo fácil completo de DESIGN_TWO_LEVELS para autoalojar.
- No afirmar velocidad, tamaño ni «más rápido que C» sin corrida medida.
- No tratar el material de `docs/archive/` (Chronos) como progreso actual.
- No abrir ARM64 u otros targets bajo esta propuesta.
- No introducir Python ni herramientas fuera del estilo del repo (Make, C, Copihue, shell).

## 6. Siguiente paso (aprobado el 2026-11-05)

Una sola fase, acotada: **S0**.

Entregable: inventario de módulos + corpus de igualdad + lista cerrada de huecos que alimentan S1, escrito y acordado (actualización de este documento). Sin árbol `compiler/self/`, sin cambios de lenguaje, sin commits de reescritura.

**Hecho (2026-11-05), falta la firma:** el entregable está en [SELFHOST_S0.md](SELFHOST_S0.md) y en `tools/selfhost/`. No hay `compiler/self/`, no hubo cambios de lenguaje ni de compilador, `LANG_VERSION` sigue en 0.44.8. S0 queda cerrada cuando Ignacio revise el anexo y resuelva sus decisiones (§4 del anexo).

Cuando S0 esté firmado, el siguiente ticket es **S1**: devolver `str`/`[]T` y `str`/`[]T` como campo de struct (la lista cerrada del anexo §3.1; `vec` en structs queda fuera y se cubre con `Vec<T>`). Antes de S2 queda pendiente el segfault del bootstrap C con `p->campo[i]` (anexo §3.4; corregido en 0.45.1).

**S1 hecha (2026-11-10), falta la revisión:** Copihue 0.45.0, rama `selfhost/s1`. Detalle en [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md). Antes de S2 siguen pendientes el segfault de `p->campo[i]`, SEC-1 ([DESIGN_SECURITY.md](DESIGN_SECURITY.md)) y los hallazgos de [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md) §7 que afecten al código del compilador en Copihue (un literal de struct como argumento y un campo struct pasado por valor generan código incorrecto sin error).

**0.45.1 (2026-11-10), rama `fix/mem-corruption`:** el segfault de `p->campo[i]` está corregido y **ya no bloquea S2**; también los hallazgos 1, 2, 3 y 9 de [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md) §7 (literal de arreglo local, literal de struct como argumento, struct por valor desde un campo, `&a[i]` de un arreglo local). Antes de S2 siguen pendientes SEC-1 y la decisión sobre los argumentos de línea de comandos y el oráculo (anexo S0 §4); los hallazgos 4–7 de S1 §7 no estaban en la lista de 0.45.1 y quedan anotados (el 4, `x.y` sobre algo que no es struct, da un valor falso sin aviso y conviene cerrarlo antes de que el compilador en Copihue lo pise).

## 7. Referencias

- [SELFHOST_S0.md](SELFHOST_S0.md) — fase S0: inventario de módulos, corpus de igualdad y huecos de S1.
- [DESIGN_SELFHOST_S1.md](DESIGN_SELFHOST_S1.md) — fase S1: devolver un struct, devolver y guardar `str`/`[]T` (0.45.0).
- [DESIGN_NATIVE.md](DESIGN_NATIVE.md) — ELF sin `nasm`/`ld` (M1–M6).
- [DESIGN_NATIVE_X64.md](DESIGN_NATIVE_X64.md) — codificador x86-64.
- [DESIGN_STRINGS.md](DESIGN_STRINGS.md), [DESIGN_SLICES.md](DESIGN_SLICES.md) — `str` / `[]T` y lo que aún no se puede devolver ni guardar en un struct.
- [DESIGN_TWO_LEVELS.md](DESIGN_TWO_LEVELS.md) — dos niveles; no es el camino crítico del autoalojamiento.
- [DESIGN_ENUMS.md](DESIGN_ENUMS.md), [DESIGN_GENERICS.md](DESIGN_GENERICS.md), [DESIGN_MODULES.md](DESIGN_MODULES.md) — piezas de lenguaje ya cortadas.
- README «What is missing»: *Self-hosting … are not started.*
- CHANGELOG / REFERENCE / STATUS — fuente de verdad de lo implementado; este documento no la sustituye.
