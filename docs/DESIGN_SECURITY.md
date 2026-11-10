# Diseño: seguridad (errores que no se pueden confundir con datos)

**Estado: propuesta, solo documentación.** Ninguna regla de este documento cambia hoy el compilador, las pruebas ni el lenguaje. Corte de referencia: Copihue **0.44.8** (`compiler/bootstrap-c/copihue.c`, `LANG_VERSION`; `main` en `95d3f7a`). Cada regla está marcada **Implementado** (el compilador ya lo hace, con la prueba que lo muestra) o **Propuesta** (no existe; aquí solo se describe).

Lo que este documento afirma del compilador se comprobó en el código y con programas de prueba compilados con `-S` a `-O0` y `-O2` (§2). La máquina donde se verificó es macOS y no ejecuta ELF de Linux: nada de lo de abajo se ejecutó, salvo el propio compilador. No se afirma ninguna velocidad.

## 1. Por qué este documento

En noviembre de 2026 se publicó CVE-2026-123158, apodado "\*sshole" ([sshhole.com](https://sshhole.com/), [aviso en oss-security](https://www.openwall.com/lists/oss-security/2026/11/05/3), [notas de OpenSSH 10.6p2](https://www.openssh.org/txt/release-10.6p2)). Afecta a `sshd` sobre musl (Alpine, OpenWrt, Buildroot, Yocto), sin PAM y con `PasswordAuthentication` activo, que es el valor por defecto. Hace falta una cuenta con `*` en el campo de contraseña de shadow (bloqueada: root en Alpine 3.16 o posterior, usuarios que solo entran con clave) y otra cuenta con un hash real `$5$` o `$6$`.

La cadena:

1. Desde 1980, `*` en el campo de contraseña significa "esta cuenta no acepta contraseña".
2. El estándar dice que `crypt()` devuelve `NULL` al fallar. musl (commit `b3c4cc12`, agosto de 2012) devuelve `"*"` al fallar.
3. musl limita la clave de sha512crypt a 256 bytes (commit `88bf5a8a`, 29 de agosto de 2012). Con una clave más larga, falla y devuelve `"*"`.
4. El parche de CVE-2016-6210 (enumerar usuarios midiendo tiempos) hace que `sshd`, si la cuenta no tiene un hash usable, tome el salt de **otra** cuenta (`pick_salt`) y calcule igual, para gastar el mismo tiempo.
5. Con una clave de 257 a 1024 caracteres, `crypt` falla y devuelve `"*"`. `strcmp("*", "*") == 0`. Entra.

Se corrigió en OpenSSH 10.6p2, publicado el 2026-11-05. El commit `4353fdc43ef646c43a8dda7025cc1a898caf1482` de openssh-portable compara contra un largo mínimo de hash (`MIN_PASSWORD_HASH_LEN`) en vez de confiar en el `"*"` de musl. La mitigación es `PasswordAuthentication no`. Hay reportes de explotación activa (Ars Technica, noviembre de 2026).

Cuatro errores de diseño, no uno:

- **Un error se codificó como un valor del dominio.** `"*"` parece un hash; es un fallo.
- **Un estado ("bloqueada") usaba el mismo valor** que el error.
- **Una comparación de strings no podía distinguirlos.** `strcmp` solo ve bytes.
- **El arreglo de tiempos usó material de otro principal** (el salt de otra cuenta).

Ignacio: "Ese tipo de estupideces no debemos permitir". Este documento baja eso a reglas de Copihue.

## 2. Estado actual de Copihue (verificado)

Las sondas viven en `build/s19/` (ignorado por git, no forma parte del repositorio). Se compilaron con el bootstrap construido con `gcc -O2 -Wall -Werror`.

### 2.1 ¿Se puede descartar un `Result` sin usarlo?

**Sí, sin error ni advertencia.** `check(5);` con `fn check(n: i64) -> Result<i64, i64>` compila a `-O0` y `-O2`, y también `look(0);` con un enum propio de tres variantes. `let r = check(0);` sin leer `r` también compila. La referencia lo dice: "A discarded `give();` allocates a temporary" ([REFERENCE.md](REFERENCE.md), Enums). En el código, la llamada que devuelve enum va a `gen_sret_call` sin mirar si es una sentencia suelta (`copihue.c:8497`, `gen_user_call`).

El precedente existe para otro caso: descartar una llamada que devuelve `own *T` **sí** es error (`an owning pointer must be moved into 'own *T'`, `copihue.c:8489`, con `g_stmt_expr` de `copihue.c:964`; `tests/errors/own_disc.lap`). La regla R3 reutiliza ese mismo punto.

### 2.2 ¿`str == str` compara largo y luego bytes con salida temprana?

**Sí.** [REFERENCE.md](REFERENCE.md), Strings: `a == b` compara el largo y después los bytes, solo entre nombres de `str` o `[]u8`. Antes del frame se reescribe como el `if` y el `for` que se escribiría a mano (`append_byte_loop`, `build_return_eq` y `build_local_eq`, `copihue.c:20089`–`20180`). El código que sale depende de la forma:

- `return a == b`: si los largos difieren, `ret` de inmediato; en el primer byte distinto, `ret` de inmediato. El tiempo depende del largo y de la posición de la primera diferencia.
- `let x = a == b`: si los largos difieren, salta al final. Si son iguales, recorre **todos** los bytes, pero cada byte distinto toma otra rama (`je` / `mov r14, 0`). No hay salida temprana en el recorrido, pero sí ramas que dependen del contenido.

Ninguna de las dos formas es de tiempo constante, ni pretende serlo. El segundo índice además lleva la comprobación de límites (`jae .Lslice_trap_N`), que depende solo de los largos. No hay `memcmp`.

### 2.3 ¿Hay una comparación en tiempo constante?

**No.** No hay builtin ni función de biblioteca con ese fin (`grep` de `ct_eq`, `memcmp` y "constant time" en `compiler/`: nada fuera de `memcmp` del propio C). `strcmp` del runtime (`__strcmp`, `copihue.c:20877`) sale en el primer byte distinto. Su comentario dice que devuelve -1, 0 o 1; el código devuelve la resta de los dos bytes.

### 2.4 ¿Un `*u8` o un `str` de retorno puede mezclar "hash", "bloqueado" y "falló"?

**Con `*u8`, sí, y compila hoy.** La sonda `t9_ptr_cmp.lap` escribe el error de SSHole tal cual: una función `-> *u8` que devuelve `"*"` cuando falla, una entrada bloqueada `"*"` y `if (strcmp(got, stored) == 0)`. Compila sin aviso. También compilan `got == stored` (compara direcciones) y `got == n` con `n: i64` (comparar un puntero con un entero no es un error de tipos: "Not checked: ... comparisons as a type error", [REFERENCE.md](REFERENCE.md), Type checking). Un `str` no se puede devolver (`a slice cannot be returned`), así que ese camino hoy no existe.

**Con enums, no.** Los tipos que lo evitan ya existen:

```copihue
enum Shadow { Locked, Hash(*u8, i64) }
enum Crypt { Digest(*u8, i64), TooLong(i64), BadSetting }
```

`strcmp(c, entry)` con esos dos es `argument 1 of 'strcmp': expected *u8, found Crypt` y `an enum is not a scalar; match it`. `c == entry` es el mismo error dos veces (sonda `t18_sshole_enum.lap`). Un enum no se compara con `==`, ni con otro enum ni con el mismo (`t4_enum_eq.lap`, `t5_enum_same.lap`; `copihue.c:14069`, `gen_expr`).

Cuánto cuesta escribirlo hoy: la sonda `t12_today.lap` (verificación completa con `Shadow`, `Crypt` y `Auth`) compila a `-O0` y `-O2` en 45 líneas. Los costos concretos:

- `Result<T, E>` no sirve para el error de `crypt`: un campo de enum no puede ser otro enum, así que `Err` lleva un `i64` o un puntero ([DESIGN_ERRORS.md](DESIGN_ERRORS.md)). Con un enum propio (`Crypt`) se pierde `?`, que pide variantes `Ok` y `Err`.
- Un brazo de `match` produce un escalar. Para llegar a `Auth` hay que pasar por un `i64` y volver.
- Un campo de enum no puede ser `str`: el hash viaja como puntero y largo.
- `slice()` solo vale en un `let`, así que comparar los bytes pide una función auxiliar que arme los dos `[]u8`.

### 2.5 Centinelas que existen hoy

En el runtime y los builtins (lo que un programa recibe):

| Dónde | Qué devuelve al fallar | Por qué importa |
|---|---|---|
| `/` y `%` por cero | **0** (`test rbx, rbx` / `jnz`; `copihue.c:12775`, `emit_binop_rax_rbx`; [REFERENCE.md](REFERENCE.md), `div_raw`) | 0 es un cociente válido. Es el mismo patrón que SSHole: un error con forma de dato. `div_raw` y `rem_raw` son la forma explícita, que hace `#DE`. |
| `malloc(n)` | según el comentario, -1 (`copihue.c:9656`) | Ver el hallazgo de §2.7. |
| `open`, `read`, `write`, `close`, `syscall6` | el resultado crudo de Linux: `-errno` en el mismo `i64` que el descriptor o la cuenta | Error y dato comparten el canal. |
| `sched_setaffinity(cpu)` | -1 si `cpu >= 1024` (sin llamar al kernel), `-errno` si el kernel falla ([REFERENCE.md](REFERENCE.md), Hosted threads) | Dos fuentes de error en un mismo número. |
| `thread_spawn` | el id del hilo o un `-errno` | Igual. |
| `strcmp` | 0 si son iguales | `strcmp(a, b) == 0` es la línea de SSHole. |
| `compare_exchange` | el valor viejo, no un booleano | El llamador compara con `expected`. Documentado; no es un error, pero es fácil de leer mal. |
| `rdrand(&ok)`, `rdseed(&ok)` | valor 0 y `ok = 0` | Buen ejemplo: el estado va aparte del valor. Ignorar `ok` sigue siendo posible. |

En el propio compilador (C): `symtab_lookup` devuelve 0 como "no está" (`copihue.c:1247`; un desplazamiento 0 nunca es un local válido, por eso funciona); `typetab_field_offset` devuelve -1 (`copihue.c:833`); `type_layout` devuelve -1 (`copihue.c:660`); `arg_class` codifica cuatro resultados en `1, 0, -1, -2` (`copihue.c:7416`); el plegado de constantes devuelve -1 para "divide por cero" junto al valor en un puntero de salida (`copihue.c:1863`). El comentario de `type_layout` (`copihue.c:655`) muestra que el proyecto ya sacó una vez un valor por defecto silencioso ("guessing 8 would be the old silent wrong size"); `type_size` (`copihue.c:1253`) todavía devuelve 8 para un nombre que no conoce (no se rastreó si algún camino actual llega ahí con un nombre desconocido).

Tras un `diag_error` el generador emite `xor rax, rax` y sigue, para juntar más errores. Eso no es un centinela que llegue al programa: con un error no se escribe el archivo de salida y el compilador sale con 1.

### 2.6 Tipos nominales, newtypes y comparación

- **No hay alias ni newtype.** No existe `type X = ...` (`grep` en `copihue.c`: nada).
- **Dos enums distintos:** no se comparan (ni dos del mismo tipo: §2.4). Pasar un enum de otro tipo a un parámetro es `argument 1 of 'use_hash': expected Hash, found Locked` (`t14_enum_newtype.lap`). Un enum de una sola variante (`enum Hash { H(*u8) }`) ya funciona como newtype nominal: se pasa, se devuelve por el puntero oculto y se desarma con `match`.
- **Dos structs distintos:** asignar uno al otro es error (`cannot assign struct 'Locked' to struct 'Hash'`, `copihue.c:12005`; `t8_struct_assign.lap`). Pero:
  - **`h == k` compila**, sean del mismo tipo o no (`t6_struct_eq.lap`, `t7_struct_same.lap`). Compara las **direcciones** de los dos locales (`lea rax, [rbp-8]` / `lea rax, [rbp-16]` / `cmp`), no el contenido. Un struct usado como valor es su dirección (`copihue.c:14074`, `gen_expr`).
  - **Pasar un `Locked` a un parámetro `h: Hash` compila** (`t13_newtype.lap`). `check_types` vuelve sin comparar nada cuando cualquiera de los dos lados es un struct (`copihue.c:6221`), así que tampoco se rechaza un `*u8` pasado a un parámetro struct.
  - Por eso un struct de un campo **no** sirve hoy como newtype seguro.

### 2.7 Hallazgos que no se esperaban

Ninguno se corrigió aquí: este cambio es solo documentación.

1. **`malloc` compara el resultado de `mmap` contra -1** (`copihue.c:9657`, y lo mismo en `emit_malloc_rax`, `copihue.c:9961`, que usa `vec_push`). La syscall cruda de Linux no devuelve -1: devuelve `-errno` (por ejemplo, -12 para `ENOMEM`). Con -12 el salto no se toma, se escribe el tamaño en la dirección -12 y el proceso cae por segmentación en vez de devolver el centinela o escribir `out of memory`. El mismo `vec_push` sí trata bien a `mremap` (`test rax, rax` / `js`, `copihue.c:10103`), así que los dos caminos no son coherentes. Leído en el código; no ejecutado.
2. **`==` entre structs compara direcciones** (§2.6).
3. **Un struct de otro tipo pasa como argumento** (§2.6).
4. **`return p` de un struct local devuelve la dirección de su slot en el frame** (`lea rax, [rbp-8]`, `leave`, `ret` en `t13_newtype.lap`). La referencia dice que el tipo de retorno struct "is parsed and is not checked" ([REFERENCE.md](REFERENCE.md), Declarations). `tests/test_struct_return.lap` usa esa forma: llama `make_point(10, 20)` y `make_point(99, 88)`, y su salida esperada (`tests/expected/tests/test_struct_return.out`) guarda `p1.x = 7`, `p1.y = 7`, `p2.x = 7`, `p2.y = 7`. Esa salida no corresponde al programa: el golden guardó lo que quedó en la memoria. Esto contradice [SELFHOST_S0.md](SELFHOST_S0.md) §3.3, que da ese archivo como prueba de que un struct "sí se devuelve por valor" (estilo de respaldo `struct Str`). No se ejecutó aquí.
5. **El compilador cae a veces, no siempre**, con un `let x: []u8 = slice(...)` y un `==` sobre ese local dentro de un brazo bloque de `match`. La caída depende de la corrida (cambia con el nombre del archivo de salida y de una vez a otra), lo que apunta a una lectura de memoria no inicializada o fuera de rango. En 20 compilaciones por caso:
   - `t11_seg_a.lap` (20 líneas, dos `slice()` y `got == want`): a `-O0`, 11 segfault (139) y 9 bien; a `-O2`, 20 bien.
   - `t11_seg_e.lap` (un solo `slice()` y `got == got`): a `-O0`, 6 SIGBUS (138) y 14 bien; a `-O2`, 20 bien.
   - `t10_today.lap` (una versión anterior de `t12_today.lap` que además pasa un literal a un parámetro `str`, cosa que el compilador rechaza en la línea 39): a `-O0`, 20 segfault; a `-O2`, 11 segfault y 9 veces el mensaje correcto `expected []u8, found *u8`. O sea, `-O2` tampoco está libre.
   - Sin `match` (`t11_seg_c.lap`, lo mismo en un `if`), sin `==` (`t11_seg_b.lap`) o con `==` sobre parámetros `str` (`t11_seg_d.lap`): 20 de 20 bien a ambos niveles.

   No se buscó la causa: no se corrió el bootstrap con AddressSanitizer. No se sabe si tiene relación con el segfault de `p->campo[i]` de [SELFHOST_S0.md](SELFHOST_S0.md) §3.4.
6. **Un límite interno sin su propio error:** la bajada de `==` sobre `str` registra hasta 128 nombres por función (`copihue.c:20269`). Con 130 locales `str`, comparar el 130.° da `a slice is not a scalar; read .ptr or .len` (`t17_many_str.lap`), que no dice cuál es el límite. Es un error, no un fallo silencioso, pero el mensaje engaña (ver R5).

Comprobado y descartado: con 13 comparaciones de `str` en una sola sentencia (tope de 24 sentencias izadas, `copihue.c:20202`) la sentencia se vuelve a procesar y las 13 quedan bajadas (13 trampas de índice en `t16_many_cmp.lap`). No es un hallazgo.

## 3. Reglas propuestas

Los dos niveles son los de [DESIGN_TWO_LEVELS.md](DESIGN_TWO_LEVELS.md): **fácil** es lo que el programador obtiene sin escribir nada; **explícito** es cómo se sale de la regla, o cómo pide más control, de forma visible en el código.

Recuento usado abajo: una copia instrumentada del bootstrap, solo en `build/s19/instr/` (no en `compiler/`), escribió una marca por cada llamada a función que devuelve enum usada como sentencia, y por cada struct local usado como valor. Se pasaron los 571 `.lap` de `tests/` y `examples/` con `-S -O0`. Resultado: **0** descartes de enum y **0** comparaciones entre structs (las 6 marcas fueron arreglos de structs que decaen a puntero, la prueba de error `tests/errors/generic_op.lap` y el retorno de struct de `tests/test_struct_return.lap`). Un `grep` de llamadas sueltas a cada función `-> Result` del corpus da lo mismo.

### R1. Un error no es un valor del dominio

**Propuesta.** Una función que puede fallar devuelve un enum con una variante de error (`Result` o uno propio). Nunca un `-1`, un `0`, un `NULL`, un `""` o un `"*"` que también podría ser un resultado válido.

- **Hoy:** el lenguaje lo permite (enums, `match` exhaustivo, `?`; [DESIGN_ERRORS.md](DESIGN_ERRORS.md)) y `examples/parse_result.lap` ya reemplaza un entero mágico por `Result`. Pero los builtins de §2.5 usan centinelas, y el compilador no puede saber si un `-1` de una función del usuario es un error o un dato.
- **Falta:** (a) una regla escrita para el código nuevo; (b) decidir qué pasa con los builtins existentes; (c) poder poner un enum dentro de `Err` (hoy un campo de enum es un entero o un puntero, así que el tipo de error termina siendo un código `i64`).
- **Cómo:**
  - Código nuevo de la biblioteca y del compilador en Copihue (desde S2): regla de revisión, sin chequeo del compilador. El compilador en C ya muestra el costo de no hacerlo (`typetab_field_offset` con -1, `arg_class` con cuatro códigos).
  - Builtins: no se cambian en silencio. Para devolver un `Result` desde un builtin el compilador tendría que conocer `Result`, y hoy no lo conoce (pregunta abierta 1). Mientras tanto, cada builtin con centinela queda listado en REFERENCE con su forma explícita (`div_raw` ya lo es para `/`).
  - El `malloc` de §2.7 es un error de implementación y se arregla aparte (comparar con el rango `-4095..-1`), sin esperar a esta regla.
- **Fácil:** la biblioteca devuelve enums; `?` propaga. **Explícito:** el builtin crudo (`syscall6`, `div_raw`) para quien quiere el número tal cual.
- **Rompe:** nada mientras sea regla para código nuevo. Cambiar la firma de los builtins rompería a quien los usa: en `tests/` y `examples/`, `malloc` aparece en 26 archivos, `thread_spawn` en 13, `read`/`write`/`close` en 7 cada uno, `sched_setaffinity` en 4, `strcmp` en 3 y `open` en 1 (`grep` de la llamada; puede incluir funciones del usuario con el mismo nombre).
- **Fase:** regla de estilo del compilador en Copihue desde **S2** (no cambia la lista cerrada de S1). Los builtins, en un corte aparte después de **S5**, si Ignacio lo aprueba.

### R2. "Bloqueado" y "falló" son tipos distintos; compararlos no compila

**Implementado para enums. Propuesta para structs.**

- **Hoy:** si "bloqueado" es `Shadow::Locked` y "falló" es `Crypt::TooLong(n)`, no hay `==` posible entre ellos ni con un `*u8` (§2.4, `t18_sshole_enum.lap`). Un enum de una variante sirve de newtype nominal (§2.6).
- **Falta:** cerrar los dos agujeros de structs. `h == k` entre structs compila y compara direcciones; un struct de otro tipo pasa como argumento.
- **Cómo, en el bootstrap C:**
  - En `gen_expr`, un `AST_COMPARE` cuyo operando es un struct (local, global o campo; no un arreglo) da `a struct is not a scalar; compare the fields`, con el mismo estilo que `an enum is not a scalar; match it` (`copihue.c:14069`). `&h == &k` sigue valiendo: es comparar punteros, y se escribe a propósito.
  - En `check_types` (`copihue.c:6221`), un destino struct exige el mismo struct, como ya se hace con enums unas líneas antes: `argument 1 of 'f': expected Hash, found Locked`.
  - En el compilador en Copihue, la misma regla vive en el módulo de chequeo que S3 porta; la igualdad con el C cubre los `.err` de las pruebas de error.
- **Fácil:** modelar los estados con un enum; el compilador impide mezclarlos. **Explícito:** comparar los campos uno por uno, o comparar direcciones con `&`.
- **Rompe:** `==` entre structs, 0 programas (recuento instrumentado). Para el chequeo de structs se marcó, en la misma copia instrumentada, cada vez que `check_types` vuelve con un struct de un lado y otro tipo del otro. En los 571 archivos salen 2 marcas:
  - `tests/errors/generic_op.lap:4`, un `return a + b` con destino `Point`. Es una prueba de error que ya falla por otra razón.
  - `tests/test_parser_pass.lap:44`, `read_token(parser)` con `parser: Parser` y el parámetro `p: *Parser`. Pasa porque un struct usado como valor es su dirección (§2.6). Está en el corpus de S0 (`tools/selfhost/corpus.txt`).

  Con la regla tal como se propone (un destino **struct** exige el mismo struct) se rompen 0 programas. Si además se prohíbe que un struct pase a un parámetro puntero sin `&`, se rompe 1, `test_parser_pass.lap`, que habría que escribir `read_token(&parser)` (pregunta abierta 8).
- **Fase:** corte aparte, **después de S1 y antes de S2** (lo llamo SEC-1 abajo). Así el primer módulo del compilador en Copihue (S2) ya se escribe con la regla activa.

### R3. Un `Result` no usado es error de compilación

**Propuesta.**

- **Hoy:** se descarta en silencio (§2.1).
- **Cómo, en el bootstrap C:** en `gen_user_call`, junto al chequeo de `own *T` (`copihue.c:8489`): si la función devuelve un enum y la llamada es la sentencia entera (`g_stmt_expr == n`), error `the result of 'check' is discarded; match it, use '?', or write 'let _ = check(...)'`. Lo mismo para una llamada indirecta que devuelva enum (hoy ya es error por otra razón). `let r = check(0);` sin leer `r` no entra en esta regla: detectar un local no leído es otro análisis (pregunta abierta 3).
- **Cuál enum.** Dos opciones:
  - (a) Solo los de forma `Ok(T)` / `Err(E)`, con la misma prueba estructural que ya usa `?` (no mira el nombre `Result`, [DESIGN_ERRORS.md](DESIGN_ERRORS.md)).
  - (b) Cualquier enum devuelto. Un enum devuelto es casi siempre una decisión; un `Option` descartado a propósito se escribe con `let _`.
  - Recomiendo (b): una regla más simple y sin excepciones por forma. Las dos rompen 0 programas del corpus.
- **Descarte explícito:** `let _ = check(5);`. Hoy `let _` es un local común llamado `_` (un segundo `let _` en el mismo bloque es `redeclaration of '_'`, `t15_let_underscore.lap`). La propuesta: `let _ = e;` evalúa `e`, no crea nombre y se puede repetir. `_` ya es el comodín de `match`, así que encaja con el lenguaje. El corpus no usa `_` como nombre de variable (`grep` de `let _` y `_ =`: 0). La otra forma posible es `_ = check(5);`: hoy es `assignment to undeclared variable '_'` (`t12_underscore_assign.lap`, sin otros errores), así que esa sintaxis está libre y no rompería nada. Recomiendo `let _`, que se lee como una declaración y no como una asignación.
- **Fácil:** el resultado hay que usarlo; si no, no compila. **Explícito:** `let _ =` deja constancia de que se ignoró a propósito.
- **Rompe:** 0 programas de `tests/` y `examples/` (recuento instrumentado y `grep`). Cambia una frase de REFERENCE ("A discarded `give();` allocates a temporary").
- **Fase:** SEC-1, con R2.

### R4. Igualar tiempos nunca con material de otro principal

**Propuesta. El compilador no puede comprobarla.**

- **Hoy:** Copihue no tiene biblioteca de autenticación ni de cifrado. No hay nada que corregir; hay que dejarlo escrito antes de que exista.
- **Regla:** cuando una cuenta no tiene hash usable (bloqueada, inexistente, sin contraseña), el cálculo de relleno usa un valor sintético propio del proceso: un salt generado al arrancar (con `rdrand`/`rdseed` y su `ok`, o con el `getrandom` del kernel por `syscall6`), nunca el salt, el hash ni ningún dato de otra cuenta. El resultado de ese cálculo se descarta por construcción: la función de relleno no devuelve nada que se pueda comparar.
- **Cómo:** en la biblioteca, cuando exista: `fn burn_time(key: str)` sin valor de retorno útil, separada de `verify`. Los tipos ayudan un poco: si `Shadow::Locked` no lleva salt, no hay de dónde sacar uno "prestado" sin ir a buscar otra entrada a propósito. Lo demás es revisión de código.
- **Fácil / explícito:** no aplica al compilador; en la biblioteca, `verify` hace el relleno sola.
- **Rompe:** nada.
- **Fase:** ninguna de S0–S5. Va con la primera biblioteca que maneje credenciales.

### R5. Un límite de largo tiene su propio `Err`

**Propuesta para la biblioteca. Implementado en parte en el compilador.**

- **Hoy, en el compilador:** varios límites ya son errores con nombre: `too many monomorphized copies` (64), `variant '...' is too large`, `too many sections`, `an array length must be from 0 to 2147483647`. El contraejemplo es §2.7 punto 6 (128 locales `str` terminan en un mensaje que no nombra el límite).
- **Hoy, en programas:** el índice de un slice fuera de rango no se trunca: sale con `index out of range` (hosted). Un literal que no cabe es error. Una conversión que corta bits exige `as`. Pero la aritmética `i64` da la vuelta sin aviso, y `/` por cero da 0 (§2.5).
- **Regla:** una función con un límite de entrada lo declara y devuelve una variante propia (`Crypt::TooLong(n)`) antes de hacer nada. No trunca la entrada, no devuelve un valor "seguro" y no reutiliza la variante de otro error.
- **Cómo:** en la biblioteca, regla de revisión. En el bootstrap C, el mensaje de §2.7 punto 6 se arregla en un parche chico (decir `more than 128 str locals in one function`); en el compilador en Copihue, desde S2, cada tabla con tope tiene su diagnóstico.
- **Fácil:** el límite aparece como una variante que `match` obliga a tratar. **Explícito:** quien quiera truncar lo escribe (`s[0..256]`).
- **Rompe:** nada.
- **Fase:** la parte del compilador, desde **S2**; el mensaje del bootstrap, en cualquier momento.

### R6. Comparación en tiempo constante

**Propuesta.**

- **Hoy:** no existe (§2.3).
- **Forma:** builtin `ct_eq(a, b)` con `a` y `b` dos `[]u8` o `str`, que devuelve `bool` (0 o 1). El nombre no choca con nada del corpus (`grep`: 0). Hosted, `--freestanding` y `--flat`, como `==` sobre `str`.
- **Semántica:**
  - El **largo es dato público**: si `a.len != b.len`, devuelve 0 sin mirar los bytes. Quien necesite ocultar el largo compara digests de largo fijo.
  - Con largos iguales recorre **todos** los bytes y acumula `acc = acc | (a[i] ^ b[i])`, sin salida temprana y sin ninguna rama que dependa del contenido. Al final, `acc == 0`.
  - No hay comprobación de índice dentro del recorrido: el largo ya se comparó y el índice es el contador.
- **Cómo, en el bootstrap C:** una rutina del runtime escrita en ensamblador, `__ct_eq`, emitida solo si el programa la usa, como `__strcmp` (`copihue.c:20877`). No una bajada a `if`/`for` como la de `==`: así ninguna pasada de `-O2` le puede cambiar la forma. El cuerpo: `movzx eax, byte [rdi+rcx]`, `xor al, [rsi+rcx]`, `or dl, al`, `inc rcx`, `cmp rcx, rdx_len`, `jb`. El único salto condicional del recorrido depende del contador. En el compilador en Copihue (S3/S4), el generador emite los mismos bytes; la igualdad byte a byte con el C lo cubre.
- **Cómo se prueba (sin medir tiempos):**
  - Funcional, a `-O0` y `-O2`: iguales, distintos en el primer byte, distintos en el último, largos distintos, los dos vacíos.
  - Estructural: un script (sh/awk) lee el asm de `__ct_eq` con `-S` y exige que dentro del recorrido no haya `je`, `jne`, `jz`, `jnz`, `cmov` ni `set*` que lean un registro cargado de los datos; solo el `jb` del contador.
  - Un `.err` para los mal usos (`ct_eq` de un `*u8`, de un `[]i64`).
  - No se afirma tiempo constante medido. Una prueba estadística de tiempos (estilo dudect) se podría agregar, y solo si corre de verdad se escribe lo que midió y en qué máquina.
- **Fácil:** con R2 activo, un struct `Secret` de la biblioteca (un `str` como campo, que llega en S1) no se puede comparar con `==` (no compila), y su única función de comparación usa `ct_eq`. **Explícito:** `ct_eq` directo sobre dos `[]u8`. `==` sobre `str` sigue siendo la comparación rápida, para datos públicos (un parser).
- **Rompe:** nada; es nuevo.
- **Fase:** corte aparte, SEC-2. No depende del autoalojamiento y puede ir antes de S2. El tipo `Secret` depende de S1 (`str` como campo) y de SEC-1 (`==` sobre struct es error).

### Resumen de fases

| Corte | Qué | Cuándo |
|---|---|---|
| Parche | `malloc`/`vec_push` contra `-errno`; mensaje del tope de 128 `str` | En cualquier momento; son errores de implementación. |
| SEC-1 | R2 (`==` entre structs es error; argumento struct nominal), R3 (enum descartado es error; `let _`) | Después de S1, antes de S2. |
| SEC-2 | R6 (`ct_eq`) | En cualquier momento; antes de cualquier código que compare secretos. |
| S2 en adelante | R1 y R5 como reglas del compilador en Copihue | Con S2. |
| Biblioteca | R4, R5, tipo `Secret` | Con la primera biblioteca que maneje credenciales; `Secret` después de S1 y SEC-1. |
| Builtins | R1 sobre los builtins existentes | Después de S5, si se aprueba. |

## 4. Relación con `DESIGN_ERRORS.md`

[DESIGN_ERRORS.md](DESIGN_ERRORS.md) define el mecanismo: `Result` lo escribe el programa, `?` es el `match` de siempre, no hay excepciones, ni `From`, ni `panic`. Este documento no cambia nada de eso. R3 usa la misma prueba estructural que `?` (o la amplía a todo enum, según la opción que se elija) y no hace de `Result` un tipo del compilador.

Hay tres tensiones, que dejo como preguntas y no resuelvo:

1. **`Result` no es un tipo del compilador.** R1 aplicado a los builtins necesitaría que el compilador devuelva un `Result` que el programa no declaró. Eso contradice la primera línea del diseño de errores.
2. **`Err` no puede llevar un enum.** DESIGN_ERRORS lo deja fuera ("Un `Ok` cuyo campo sea un enum"; un campo de enum no puede ser un enum). Comprobado: `Result<i64, AuthErr>` con `AuthErr` un enum es `type argument 'AuthErr' is not an integer, a pointer, or a struct` (`u1_result_enum_err.lap`). Con esa limitación, el tipo de error de R1 es un código `i64` dentro de `Err`. No es un centinela del dominio, porque vive en la otra variante, pero pierde lo que `match` daría con un enum de errores.
3. **`?` exige `Ok` y `Err`.** Un enum de dominio como `Crypt { Digest, TooLong, BadSetting }` expresa mejor los estados, pero no se propaga con `?`.

## 5. Ejemplo SSHole en Copihue

El error, escrito con lo que hay hoy, **compila** (resumen de la sonda `t9_ptr_cmp.lap`):

```copihue
fn crypt_like(n: i64) -> *u8 {
    if (n > 256) { return "*"; }        // falla: devuelve algo con forma de hash
    return "$6$salt$hash";
}

fn main() -> i32 {
    let stored: *u8 = "*";              // cuenta bloqueada
    let got: *u8 = crypt_like(300);
    if (strcmp(got, stored) == 0) { print_int(1); }   // entra
    return 0;
}
```

La misma verificación con las reglas. **Propuesta:** usa `ct_eq` (R6), `let _` (R3) y `Err` con un enum (pregunta abierta 2), que no existen. Lo demás (enums, `match`, `?`) existe en 0.44.8.

```copihue
enum Shadow { Locked, NoPassword, Hash(*u8, i64) }   // tres estados, tres variantes
enum CryptErr { TooLong(i64), BadSetting }
enum Result<T, E> { Ok(T), Err(E) }
enum Auth { Granted, Denied }

const MAX_KEY = 256;

// R5: el límite tiene su propia variante. No hay "*" posible.
fn crypt_sha512(key: str, setting: *u8, slen: i64, out: *u8) -> Result<i64, CryptErr> {
    if (key.len > MAX_KEY) { return Result<i64, CryptErr>::Err(CryptErr::TooLong(key.len)); }
    // ... calcula en out y devuelve el largo del digest
    return Result<i64, CryptErr>::Ok(86);
}

// R4: relleno de tiempo con un salt sintético del proceso, no de otra cuenta.
// No devuelve nada comparable.
fn burn_time(key: str) {
    let _ = crypt_sha512(key, &dummy_setting as *u8, DUMMY_LEN, &scratch as *u8);
}

fn check_hash(key: str, p: *u8, n: i64) -> Result<i64, CryptErr> {
    let dn = crypt_sha512(key, p, n, &digest as *u8)?;
    let got: []u8 = slice(&digest as *u8, dn);
    let want: []u8 = slice(p, n);
    return Result<i64, CryptErr>::Ok(ct_eq(got, want));   // R6
}

fn verify(entry: Shadow, key: str) -> Auth {
    let ok = match entry {
        Locked => { burn_time(key); 0; },        // sin hash: no hay nada que comparar
        NoPassword => { burn_time(key); 0; },
        Hash(p, n) => {
            let r = check_hash(key, p, n);
            match r { Ok(same) => same, Err(e) => 0 };
        }
    };
    if (ok == 1) { return Auth::Granted; }
    return Auth::Denied;
}
```

Por qué el error de SSHole no compila aquí:

- **El fallo no es un string.** `crypt_sha512` devuelve `Result<i64, CryptErr>`. `strcmp(r, entry)` es `expected *u8, found Result...` y `an enum is not a scalar; match it`. **Implementado:** es el mensaje de hoy para cualquier enum (`t18_sshole_enum.lap`).
- **"Bloqueada" no se compara con "falló".** `Shadow::Locked` y `CryptErr::TooLong` son de enums distintos; `==` entre enums no compila. **Implementado.**
- **"Bloqueada" no tiene hash.** El brazo `Locked` no liga nada que se pueda pasar a `ct_eq`. Para comparar habría que inventar un valor a propósito, a la vista. **Implementado** (los brazos de `match` solo ligan los campos de su variante).
- **No se puede olvidar el error.** Si alguien escribe `crypt_sha512(key, p, n, out);` y sigue, no compila. **Propuesta** (R3).
- **No hay salt prestado.** `Locked` no lleva salt; `burn_time` usa el sintético. **Propuesta, y no la comprueba el compilador** (R4).
- **La comparación de los bytes no filtra la posición.** `ct_eq`. **Propuesta** (R6).

## 6. Preguntas abiertas para Ignacio

1. ¿Puede el compilador conocer un `Result` propio para los builtins (R1), o los builtins quedan con centinelas documentados y su forma explícita? Lo primero choca con DESIGN_ERRORS.
2. ¿Se permite un enum como campo de `Err` (y de otro enum)? Sin eso, R1 deja el tipo de error en un código `i64`.
3. R3: ¿todo enum devuelto (recomendado) o solo los de forma `Ok`/`Err`? ¿Se agrega después un aviso por local `let r = f()` nunca leído?
4. ¿SEC-1 (R2 y R3) puede ir entre S1 y S2, o se mete dentro de S1 aunque su lista esté cerrada?
5. ¿Comparar un puntero con un entero que no es literal (`got == n`) debe ser error? No se contó cuántos programas rompería.
6. Los hallazgos de §2.7 (`malloc` contra -1, `==` y argumentos struct, retorno de struct local, caída intermitente en `match`): ¿parches inmediatos, uno por uno, o se esperan a SEC-1?
7. El retorno de struct por valor (§2.7 punto 4) es el estilo de respaldo de SELFHOST_S0 §3.3 y su golden guarda basura. ¿Se corrige el compilador o se retira ese respaldo del anexo de S0?
8. Un struct pasado a un parámetro `*T` sin `&` hoy compila (`tests/test_parser_pass.lap:44`). ¿R2 lo prohíbe también (rompe ese archivo del corpus de S0) o se deja como está?
