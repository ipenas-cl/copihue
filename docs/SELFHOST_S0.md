# Autoalojamiento, fase S0: inventario, corpus y huecos

**Estado: entregado el 2026-11-05, pendiente de la revisión de Ignacio Peña.** Es el anexo de [DESIGN_SELFHOST.md](DESIGN_SELFHOST.md) §4 (S0) y §6. Es solo documentación: no hay `compiler/self/`, no hay cambios de lenguaje ni de comportamiento del compilador, y `LANG_VERSION` sigue en 0.44.8.

Corte de referencia: `main` en `c4a4036` (Copihue 0.44.8). Todas las cifras de este documento son de ese árbol y se pueden volver a sacar con los comandos que figuran al lado. Nada de aquí afirma velocidad ni tamaño de binarios.

Lo que S0 propone y espera firma (cada punto se explica abajo):

1. El orden de portado de la §1, que **corrige** el de la propuesta inicial: en el bootstrap C no hay una pasada semántica separada de la generación; los chequeos de tipos, la monomorfización y el generador son un solo bloque de ~16.700 líneas.
2. El corpus de la §2: 236 programas × `-O0` / `-O2`, ELF byte a byte, lista en `tools/selfhost/corpus.txt`.
3. Los huecos de la §3: S1 abre **dos** (devolver `str`/`[]T` y `str`/`[]T` como campo de struct). `vec T` como campo o parámetro **sale** de S1: una tabla que crece se escribe con un `Vec<T>` genérico de la biblioteca del propio compilador, que hoy compila.
4. Hallazgos del sondeo (§3.4), entre ellos un segfault del bootstrap C que no se toca en esta sesión.

---

## 1. Inventario de módulos del bootstrap C

`compiler/bootstrap-c/copihue.c` tiene **23.590 líneas** (`wc -l`) y un solo archivo C. Incluye por `#include` a `compiler/native/` (`x64.c` 920, `x64_line.c` 72, `asm.c` 786, `elf.c` 463 líneas, con `#define emit nat_emit` y `#define parse_expr nat_parse_expr` alrededor para que no choquen con los nombres del compilador).

Los encabezados `// ====` del archivo no corresponden a la estructura real (p. ej. «OPTIMIZATION HELPERS» en la línea 1632 es el inicio del parser). Los rangos de abajo salen de las definiciones de funciones, no de los encabezados:

```sh
# función, línea de inicio y tamaño (hasta la llave de la columna 0)
awk '/^[A-Za-z_][A-Za-z0-9_ \*]*[ \*][a-zA-Z_0-9]+\(.*\)[ ]*\{[ ]*$/ { name=$0; sub(/\(.*/,"",name); n=split(name,a,/[ \*]+/); fn=a[n]; start=NR }
     /^\}/ { if (start) { printf "%d %d %s\n", start, NR-start+1, fn; start=0 } }' compiler/bootstrap-c/copihue.c
```

### 1.1 Bloques

| Bloque | Líneas | Tamaño | Qué hay | Depende de |
|---|---|---|---|---|
| B0 cabecera | 1–24 | 24 | `#include`, nombre/versión/extensiones | — |
| B1 diagnósticos y tipos de datos | 25–577 | 553 | `g_src_*`, `print_source_line`, `diag_error` (variádica), nombres de módulo y de símbolo de enlace (`link_sym`, `split_link`, `focus_file`), tokens `Tok`/`Lex`, `TypeInfo`/`StructType`/`EnumType`, `AstNode` y sus ~20 banderas `AF_*`, `Symbol`, `GlobalVar`, `StringTable`, `TypeSpec`, `Parser`, `Codegen`, tabla de registros de máquina (`mach_reg`) | — |
| B2 tablas | 578–1373 | 796 | `safe_realloc`, tabla de tipos (layout de structs, `type_layout`), tabla de enums, tabla de cadenas (867–909), tabla de símbolos (1073–1251), tabla de globales (1270–1373). Entre 910 y 1072 hay ayudas de generación (`symbol_assign_reg`, `o2_hosted`, `emit_scaled_*`) que están ahí por historia | B1 |
| B3 lexer | 1374–1536 | 163 | `lex_init`, `lex_tok`. **No llama a `diag_error`** (0 usos) | B1 |
| B4 parser, AST y constantes | 1537–5325 | 3.789 | ver §1.2 | B1, B2, B3 |
| B5 chequeo de tipos + monomorfización + generador | 5326–22020 | ~16.700 | ver §1.3 | todo lo anterior |
| B6 auditoría `--pic` | 22021–22312 | ~290 | `pic_audit` relee el asm generado | B5 |
| B7 escritores de formatos y chequeo de traits | 22313–23273 | ~960 | `usage`, `run_cmd` (nasm/ld), `emit_flat_elf` (22529, 236 líneas), `check_traits` (22832), `emit_flat_measured` (22893), `emit_pe_efi` (23067, 201 líneas) | B5 |
| B8 enlace con `native` | 23274–23317 | 44 | `#include` de `compiler/native/*.c`, `write_whole_file`, `native_build` | `compiler/native` |
| B9 `main` | 23319–23590 | 272 | opciones, orden de las pasadas (abajo), caída a nasm/ld | todo |

Orden de las pasadas en `main` (líneas 23456–23545): `parse_file_into` por cada entrada → `build_enum_table` → `instantiate_structs` → `build_type_table` → `check_traits` → `codegen` (que deja el texto NASM en memoria, `g_asm_mem`) → `pic_audit` (si `--pic`) → `native_build` (asm + ELF en memoria) o los escritores flat/PE, o nasm/ld.

### 1.2 B4 por dentro

| Sub-bloque | Líneas | Tamaño | Contenido |
|---|---|---|---|
| E1 | 1537–1708 | 172 | `ast_new`, utilidades de tokens, plegado de constantes (`fold_binary_op`) |
| E2 | 1709–2013 | 305 | literales, nombres de tipo, `const_eval` y `const_find` |
| E3 | 2014–2447 | 434 | genéricos en el parser: mangling/demangling, `parse_tparams`, `parse_trait_bound` |
| E4 | 2448–3074 | 627 | `parse_match`, `parse_primary` y la cadena de precedencia hasta `parse_expr` |
| E5 | 3075–3686 | 612 | `parse_for_in`, `parse_stmt` (379 líneas), `parse_block` |
| E6 | 3687–4189 | 503 | `parse_enum_def`, `parse_struct_def`, `parse_type`, `parse_global_var`, `parse_func` |
| E7 | 4190–4586 | 397 | datos crudos, `bits`/`naked`, secciones con nombre, `declare_module` |
| E8 | 4587–5325 | 739 | `extern`, `import`, módulos, `trait`/`impl`, `const`, `parse_top_item` |

### 1.3 B5 por dentro

El chequeo de tipos, la resolución de genéricos y la generación del texto están **entrelazados**: `expr_ty`, `check_types`, `check_user_args`, `monomorphize_call` y `gen_expr` se llaman entre sí durante la misma recorrida, y los errores se emiten desde dentro de la generación. No hay una pasada «semántica» que se pueda portar y comparar antes del generador.

| Sub-bloque | Líneas | Tamaño | Contenido |
|---|---|---|---|
| F1 | 5326–5567 | 242 | `emit` (printf a un buffer), código frío, sonda de pila, `find_func` |
| F2 | 5568–6511 | 944 | tipos de expresión (`VTy`, `expr_ty`, `check_types`, `match_result_ty`, `check_arg`, `check_user_args`) |
| F3 | 6512–7249 | 738 | `check_builtin_args`, truncado y ensanchado, argumentos, llamadas con `sret` |
| F4 | 7250–8364 | 1.115 | monomorfización: `ast_clone`, `subst_*`, instanciación de structs, `infer_type_args`, `resolve_method`, `monomorphize_call` |
| F5 | 8365–9826 | 1.462 | llamadas (`ms64`, `gen_user_call`), contexto, atómicos, `gen_core_builtin` (529), `gen_builtin_call` (320) |
| F6 | 9827–11172 | 1.346 | datos, drops, `vec_push`, slices, `gen_addr_of`, expresiones de puntero |
| F7 | 11173–12027 | 855 | `gen_array_index` (436), literales y acceso a campos de struct |
| F8 | 12028–13670 | 1.643 | operandos y registros, binops, `place_*`, comparaciones, asignaciones, enums (almacén, retorno, argumento) |
| F9 | 13671–15422 | 1.752 | `gen_match` (193), `gen_expr` (**1.397** líneas) |
| F10 | 15423–18345 | 2.923 | `-O2` hosted: registros para locales (`plan_reg_locals`), marco corto, y los reconocedores de patrones `try_*` (fuerza, relleno de bytes, suma lineal, desplazamientos, copias, bumps) |
| F11 | 18346–20291 | 1.946 | `gen_stmt` (572), parámetros, `stride_while`, `try_match`, `lower_node` |
| F12 | 20292–21575 | 1.284 | `gen_func` (539), `gen_helpers`, tablas de enum/struct al texto, globales, símbolos de enlace, `gconst_eval` |
| F13 | 21576–22020 | 445 | `codegen` (el conductor) |

Las cuatro funciones más largas están en B5: `gen_expr` 1.397, `gen_stmt` 572, `gen_func` 539, `gen_core_builtin` 529. Fuera de B5, las mayores son `parse_stmt` 379, `main` 272, `emit_flat_elf` 236 y `emit_pe_efi` 201.

### 1.4 Lo que el portado tiene que reproducir de C

Medido con `grep` sobre `copihue.c` (líneas que coinciden):

| Uso | Cantidad | Qué implica |
|---|---|---|
| `emit(cg, fmt, …)` | 1.537 | formato `printf` variádico para armar el texto NASM |
| `diag_error(line, col, fmt, …)` | 495 | ídem para los mensajes |
| `snprintf(` | 174 | etiquetas y nombres armados a mano |
| `strcmp(` | 763 | comparaciones de nombres; `strcmp` ya es un builtin de Copihue |
| `strdup`/`strndup` | 282 | los nombres del AST son `char*` con NUL, copiados |
| `malloc`/`calloc`/`realloc` | 82 | |

Especificadores de formato que aparecen (`grep -o '%[-+ 0-9.#]*[lz]*[a-zA-Z]'`): `%s` 1.115, `%d` 492, `%+d` 61, `%lld` 40, `%llx` 30, `%llu` 16, `%zu` 9, `%llX` 7, `%c` 7, `%ld` 6, `%+lld` 3. `copihue.c` no contiene `double` ni `float` (`grep -w`): el portado no necesita coma flotante.

### 1.5 Orden de portado propuesto

Cada paso se compara contra el C con el artefacto que se indica. La propuesta inicial (tablas → lexer → diagnostics → parser → semántica → NASM → native) se ajusta en tres cosas: diagnostics va **antes** del parser pero no necesita ir antes del lexer (el lexer no lo usa); «semántica» y «generación» son un solo paso; y hay un paso previo de biblioteca.

| Paso | Qué | Bloques | Artefacto de comparación | Nota |
|---|---|---|---|---|
| P0 | Biblioteca del compilador en Copihue: `Buf` (texto de salida), `Vec<T>`, `strdup`, formateo de enteros | — (en C lo da la libc: `vsnprintf`, `strdup`, `realloc`) | prueba propia por separado | ver §3.3; es el único paso nuevo |
| P1 | Tablas | B2 (menos 910–1072) y los tipos de B1 | ELF de un programa de prueba que las usa (no hay volcado en C) | |
| P2 | Lexer | B3 | no hay volcado de tokens en el bootstrap: o se agrega una bandera de volcado en C (cambio de comportamiento: necesita acuerdo) o se compara el ELF de un driver que lee un `.lap` y escribe sus tokens | **decisión pendiente** (§4) |
| P3 | Diagnósticos | de B1: `print_source_line`, nombres de módulo y de enlace, `diag_error` (líneas 54–183) | `tests/expected/errors/*.err` (195 programas en `tests/errors/`) | el golden de errores ya existe; ver §2.4 |
| P4 | Parser, AST, constantes, genéricos del parser | B4 | volcado del AST (no existe) o el texto `-S` de todo el corpus con el parser en Copihue y el resto en C | requiere una frontera C↔Copihue; ver §4 |
| P5 | Chequeo + monomorfización + generador de texto NASM | B5, B6 | texto `-S` byte a byte; luego ELF | el bloque grande: se porta por sub-bloques F1…F13 en el orden de arriba, que sigue el grafo de llamadas |
| P6 | Formatos y `main` | B7, B9 | ELF y objeto `-c` del corpus | `--flat`/`--pe-efi`/`--axe*` fuera del corpus hosted |
| P7 | `compiler/native` | `asm.c`, `elf.c`, `x64.c` | ELF byte a byte | S4 de DESIGN_SELFHOST; puede quedar en C |

---

## 2. Corpus de igualdad

### 2.1 Definición

**Programas:** los `.lap` que la sección 1 de `tests/run_tests.sh` compila y ejecuta por la ruta nativa (`ls tests/*.lap tests/*/*.lap examples/*.lap examples/*/*.lap examples/*/*/*.lap`, con las mismas exclusiones y el mismo `SKIP` que esa sección), menos los que `--native` rechaza (`asm(...)`, `naked`, `interrupt fn`, `syscall fn`).

**Opciones:** `-O0` y `-O2`, sin `--nasm`, sin `--native` explícito (es el defecto). Salida `-o` a un archivo cualquiera.

**Criterio:** el ejecutable que escribe el `copihue` autoalojado es idéntico byte a byte (`cmp`) al del bootstrap C de la misma versión, para cada programa y cada nivel.

**Cifras** (con `build/s18/copihue` = `gcc -O2 -Wall -Werror` del bootstrap): 245 candidatos − 6 del `SKIP` (servidores y clientes HTTP) − 3 que `--native` rechaza (`tests/lang/cpuid_carry.lap`, `tests/lang/ms64_call.lap`, `tests/lang/naked_param.lap`) = **236 programas** (198 de `tests/`, 38 de `examples/`), es decir **472 comparaciones** (236 × 2 niveles). Por directorio, los grupos grandes: `tests/lang` 77, `examples/lowlat` 18, `tests/types` 15, `tests/global` 10; el resto son los `tests/test_*.lap` sueltos y los directorios chicos.

### 2.2 Cómo se obtiene la lista

```sh
make                                  # o: gcc -O2 -Wall -Werror -o compiler/bootstrap-c/copihue compiler/bootstrap-c/copihue.c
sh tools/selfhost/corpus-list.sh [compilador] > tools/selfhost/corpus.txt
```

`tools/selfhost/corpus-list.sh` es solo un listado, en sh: aplica los mismos globs y exclusiones que `tests/run_tests.sh` y deja pasar un programa si `--native -O0` y `--native -O2` lo compilan. No compara nada ni es un ejecutor del corpus. `tools/selfhost/corpus.txt` es la lista vigente (236 líneas, una ruta por línea); si cambia la suite, se regenera con el comando de arriba y la diferencia se revisa en el commit.

Diferencia con `tools/native/corpus.sh` (el corpus de ensamblador de DESIGN_NATIVE): ese compila con `-S`, incluye todo lo que compila con el camino que cae a nasm y excluye solo `tests/assign/kernel` y `tests/assign/store_call_flat`; el de aquí sigue la sección 1 de `run_tests.sh` y excluye `tests/assign/` entero.

### 2.3 Cómo se compara (referencia, no es un ejecutor obligatorio)

```sh
for src in $(cat tools/selfhost/corpus.txt); do
  for o in -O0 -O2; do
    "$REF"  $o -o build/ref.out  "$src" && "$SELF" $o -o build/self.out "$src" && cmp build/ref.out build/self.out || echo "DIFIERE $o $src"
  done
done
```

Con `REF` el bootstrap C y `SELF` el compilador en Copihue. Para los pasos intermedios de la §1.5 el artefacto es el texto `-S` del mismo corpus.

**Determinismo del oráculo** (comprobado con `tests/hello.lap`, en esta máquina): el mismo ejecutable con otro nombre de salida y otra ruta de entrada (`./tests/hello.lap`) sale idéntico; el objeto `-c` sale idéntico en dos corridas; el texto `-S` no contiene la ruta del fuente (solo la línea `; Generated by Copihue 0.44.8`). No se comprobó con el corpus entero ni en Linux; eso es parte del primer ciclo de S2.

### 2.4 Qué no entra, y qué queda como propuesta con acuerdo pendiente

No se amplía el corpus en silencio. Fuera por ahora:

- Los 9 de arriba (6 `SKIP`, 3 rechazados por `--native`).
- `tests/errors/` (195 programas): el oráculo es el diagnóstico (`tests/expected/errors/*.err`), no un ELF. **Propuesta:** que sea un segundo corpus de P3 en adelante.
- Objetos `-c` (`tests/link/parts/`, 21 fuentes; los `fs_` y `flat_` usan `bits 32` y secciones con nombre, que el ensamblador nativo no toma). **Propuesta:** segundo nivel, con `-c`, para S3.
- `--flat`, `--boot`, `--freestanding`, `--axe1/2`, `--pic`, `--pe-efi`, `-O1`: fuera del criterio de hecho (DESIGN_SELFHOST §3.2).
- «El propio fuente del compilador en Copihue»: lo suma S4, cuando exista.

---

## 3. Huecos del §3.1 que abre S1

### 3.1 Qué se implementa en S1 (lista cerrada)

| # | Hueco | Qué hace S1 | Por qué es imprescindible |
|---|---|---|---|
| 1 | `-> str` y `-> []T` (hoy `a slice cannot be returned`, `tests/errors/str_ret.lap`) | Se puede devolver un slice. ABI propuesta, a fijar en el DESIGN de S1: la del enum (`DESIGN_ENUMS.md`, «Devolver un enum»): el llamador pasa en `rdi` un puntero a 16 bytes de destino, los parámetros reales se corren, y el callee escribe a través de él. Ya está implementada (`gen_sret_call`, `emit_sret_rdi`) | El lexer y el diagnóstico devuelven vistas de texto; sin esto cada firma es puntero+largo a mano |
| 2 | `str` / `[]T` como campo de struct (hoy `a slice cannot be a struct field`) | El campo mide 16 bytes, alineado a 8; leer `s.f.len`, `s.f.ptr`, indexar, `==`, copiar el struct entero. Es lo que permite `struct Token { kind: i64, text: str, line: i32 }` | Un token, un símbolo y un diagnóstico llevan texto |

Cada uno trae tests de lenguaje y de error en el estilo de `tests/lang/` y `tests/errors/`, y deja `-O0` y `-O2` iguales en la suite, como siempre.

### 3.2 Qué queda fuera de S1

| Hueco | Decisión | Por qué |
|---|---|---|
| `vec T` como campo, parámetro, retorno o global | **Fuera.** Se resuelve con `Vec<T>` en la biblioteca del compilador (§3.3) | Compila hoy (sondas y1/z2 de la §3.4); no hace falta tocar el lenguaje para tablas que crecen |
| `str` en enum (`enum E { A(str) }`: `an enum field must be an integer or a pointer`) | Fuera | El AST usa índices o punteros a struct; el texto vive en un struct (hueco 2) |
| `[str; N]`, `vec str`, `Vec<str>` | Fuera (`a slice cannot be an array element`, `a vec element cannot be a slice…`) | Se envuelve: `struct Name { s: str }` y `Vec<Name>`, que usa el hueco 2 |
| `append` / capacidad de `str`, UTF-8, `String`, formato | Fuera (DESIGN_STRINGS) | Se usa `Buf` de P0 |
| Argumentos de línea de comandos | Fuera de S1; **decisión de S2** (§4) | No es un hueco del §3.1 original; ver §3.4 |
| `sizeof(T)` dentro de una plantilla genérica | Fuera; se pasa el tamaño como parámetro | ver §3.4 |
| Todo lo del §3.2 de DESIGN_SELFHOST | Fuera | |

### 3.3 Cómo escribe el compilador en Copihue las tablas y los mensajes con lo que S1 entrega

**Tablas que crecen.** Un `Vec<T>` genérico escrito en Copihue (P0), sin `vec` de lenguaje:

```copihue
struct Vec<T> { ptr: *T, len: i64, cap: i64 }

fn vec_add<T>(v: *Vec<T>, x: T, esz: i64) -> i64 {      // esz = sizeof(T), lo pasa quien llama
    if (v.len == v.cap) { vec_grow<T>(v, esz); }
    let p: *T = v.ptr;                                 // se asigna el struct entero a través de una variable puntero, no de v.ptr[i]
    p[v.len] = x;
    v.len = v.len + 1;
    return v.len;
}
```

Un struct del compilador tiene un `Vec<Sym>` como campo (la sonda y2 de la §3.4: `struct Obj { rels: Vec<Rel>, n: i64 }` compila). `Vec<Sym>` sirve para un `Sym` que sea struct; con el hueco 2, ese `Sym` puede llevar un `str`.

**Texto de salida y mensajes.** No hay `printf` ni variádicas. Un `Buf { ptr: *u8, len: i64, cap: i64 }` con `buf_put(b, s: str)`, `buf_putc`, `buf_int(b, n)` y `buf_int_signed(b, n)` (el `%+d` de `[rbp%+d]`) cubre los especificadores de la §1.4. Cada llamada `emit(cg, "    mov %s, %d\n", r, n)` pasa a una secuencia, o a un ayudante con la forma, sin cambiar el texto que sale:

```copihue
buf_put(o, "    mov "); buf_put(o, r); buf_put(o, ", "); buf_int(o, n); buf_put(o, "\n");
```

Es un portado mecánico de 1.537 + 495 llamadas, no una función del lenguaje. `diag_error` pasa a `diag_begin(line, col)` + `buf_*` + `diag_end()`, que escribe a fd 2 (`write(2, …)`) con el mismo formato `archivo:línea:col: error: …` y la línea de fuente con el signo `^`.

**Si el hueco 1 o el 2 se retrasara** (no es el plan): el estilo de respaldo es el que ya compila hoy, `struct Str { ptr: *u8, len: i64 }` definido por el programa, que sí se devuelve por valor (`tests/test_struct_return.lap`), a cambio de perder `==`, `[i]` comprobado y `print(s)` sobre ese tipo. S1 es comodidad y seguridad, no una condición de existencia del compilador en Copihue; está dicho aquí para que nadie crea lo contrario.

**Corrección (0.45.0).** El párrafo anterior estaba mal: en 0.44.8 un struct **no** se devolvía por valor. `return p` de un struct local dejaba en `rax` la dirección de su slot en el frame, y `tests/test_struct_return.lap` imprimía `7` cuatro veces en Linux; su golden había guardado esa basura ([DESIGN_SECURITY.md](DESIGN_SECURITY.md) §2.7 punto 4). Desde 0.45.0 el struct se devuelve con el puntero oculto del enum (destino en `rdi`), y el golden dice 10, 20, 99 y 88. Con los dos huecos de S1 abiertos, el respaldo deja de hacer falta.

### 3.4 Hallazgos del sondeo (solo compilación)

Se escribieron programas chicos en `build/s18/` (ignorado) y se compilaron con `-S`; **nada se ejecutó** (esta máquina es macOS y los ELF son de Linux). Los mensajes son los del compilador.

Confirman los huecos (todos con `file:line:col: error:` y `1 error, compilation failed`):

| Forma | Mensaje |
|---|---|
| `fn f() -> str` / `-> []i64` | `a slice cannot be returned` |
| `struct T { s: str }` | `a slice cannot be a struct field` |
| `struct T { v: vec i64 }`, `let g: vec i64;`, `fn f(v: vec i64)`, `-> vec i64` | `a vec must be a local` / `a vec cannot be returned` |
| `enum E { A(str) }` | `an enum field must be an integer or a pointer` |
| `[str; 2]` | `a slice cannot be an array element` |
| `vec str` | `a vec element cannot be a slice, an array, a vec, or an owning pointer` |
| `*vec i64` como tipo | no es un puntero a `vec` (`struct 'i64' has no field 'len'`) |

Compilan hoy (y el compilador en Copihue las puede usar):

- Structs con campos `*u8`, `**Node`, `*Tok`; funciones que devuelven `*Node` y structs por valor; `malloc(sizeof(S) * n)` indexado como `*S`; arrays globales de structs; tablas de punteros a función; un enum como campo de struct.
- `struct Vec<T> { ptr: *T, len: i64, cap: i64 }` con `vec_add<T>` / `vec_grow<T>` para `T = i64` y para `T` un struct (`-O0` y `-O2`), y un struct con un campo `Vec<Rel>`.
- `vec Rel` local con `vec_push` y `v[0].sym`.
- `a == b` de dos `str` y `s[i..j]`.

Restricciones de estilo que el compilador en Copihue tiene que respetar (todas con mensaje, ninguna es un hueco de S1):

- `if` y `while` piden paréntesis.
- `s[i..j]` solo en un `let` (como argumento: `expected []u8, found u8`).
- `a->b[i]->c` es un error (`'->kind' needs a variable of pointer-to-struct type on the left`): se baja a un `let k: *Node = …` intermedio.
- Asignar un struct entero a través de un índice de puntero-campo: `cannot assign a whole struct 'Tok' through a pointer expression; assign its fields` — se evita con `let p: *T = v.ptr; p[i] = x;`.
- `sizeof(T)` dentro de una plantilla es `sizeof() has no size for this type`: se pasa el tamaño como parámetro (la sonda con ese parámetro compila).

**Defecto del bootstrap C encontrado, sin tocar:** `p->campo[i]` usado como valor (`let c = b->ptr[i];`, `return b->ptr[i];`) **termina con segfault el propio compilador**, con `-O0` y con `-O2`, con cualquier nombre de campo (probado `ptr` y `data`; la forma `b.ptr[i]`, y `let q = b->ptr; q[i]`, compilan; la escritura `n->kids[0] = n` también compila). Se reprodujo con el binario `gcc -O2 -Wall -Werror` en esta máquina (Darwin); no se miró la causa ni se probó en Linux. Los tests existentes usan `.` sobre el puntero (`cont.ptr[0]`, `tests/test_ptr_to_struct_field_ptr_index.lap`), por eso la suite no lo toca. Hay que arreglarlo en el C en un ticket aparte, antes de que el código en Copihue lo use por accidente; mientras tanto, el compilador en Copihue usa `.` o un local.

Hueco que el §3.1 original no listaba: **el hosted no expone `argc`/`argv`** (`_start` hace `call main`; el lenguaje no tiene forma de leerlos, y ningún test los usa). El compilador en Copihue necesita el nombre de los archivos de entrada y las opciones. Salida sin tocar el lenguaje: leer `/proc/self/cmdline` con los builtins `open`/`read` existentes (solo Linux; no probado aquí). La alternativa es un builtin nuevo. Es decisión de S2 (§4), no de S1.

---

## 4. Decisiones que S0 deja a Ignacio

1. **S1 con dos huecos** (devolver y campo `str`/`[]T`) y `vec` de lenguaje fuera, con `Vec<T>` en P0. Si se prefiere cerrar también `vec` como campo, S1 crece; no hace falta para compilar.
2. **Argumentos:** `/proc/self/cmdline` (sin cambio de lenguaje) o un builtin. Se decide al empezar S2.
3. **Oráculo del lexer y del parser** en S2/S3: una bandera de volcado en el C (cambio de comportamiento, con test) o comparar el ELF de un driver. Hasta decidirlo, S2 no empieza.
4. **El segfault de `p->campo[i]`** en el bootstrap C: ticket aparte, previo a S2.
5. **Corpus ampliable** (§2.4): errores `.err` desde P3 y objetos `-c` desde S3.

## 5. Qué no se tocó

Ningún archivo de `compiler/`, `tests/`, `examples/`, `Makefile` ni `LANG_VERSION`. Lo nuevo: este anexo, `tools/selfhost/corpus-list.sh`, `tools/selfhost/corpus.txt`, y los cambios de texto en `docs/DESIGN_SELFHOST.md`, `README.md` y `CHANGELOG.md`. Comprobación: `gcc -O2 -Wall -Werror -o build/s18/copihue compiler/bootstrap-c/copihue.c` compila sin advertencias.
