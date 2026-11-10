# Autoalojamiento, fase S1: devolver y guardar `str` / `[]T` (0.45.0)

**Estado: entregada el 2026-11-10, pendiente de la revisión de Ignacio Peña.** Es la fase S1 de [DESIGN_SELFHOST.md](DESIGN_SELFHOST.md) §4, con la lista cerrada de [SELFHOST_S0.md](SELFHOST_S0.md) §3.1. Corte: Copihue **0.45.0** (rama `selfhost/s1`, sobre `main` en `6ac49e7`).

Lo que entró, en este orden (un commit por punto):

1. **Arreglo previo: devolver un struct por valor.** No estaba en la lista de S1; es el hallazgo de [DESIGN_SECURITY.md](DESIGN_SECURITY.md) §2.7 punto 4. Ignacio lo confirmó en Linux: `tests/test_struct_return.lap` imprimía `7` cuatro veces a `-O0` y a `-O2` porque `return p` dejaba en `rax` la dirección del slot de `p` en un frame que ya no existía.
2. **Hueco 1:** `-> str` y `-> []T`.
3. **Hueco 2:** `str` y `[]T` como campo de struct.

Nada de lo que S0 §3.2 dejó fuera entra aquí (§5). Esta máquina es macOS y no ejecuta ELF de Linux: lo que sí se ejecutó está en §6.

## 1. La ABI de retorno: un solo puntero oculto

Enum, struct y slice vuelven por el mismo camino, el que ya tenía el enum ([DESIGN_ENUMS.md](DESIGN_ENUMS.md), «Devolver un enum»; `gen_sret_call`, `emit_sret_rdi`):

- El llamador pasa en `rdi` la dirección del destino. Los parámetros reales se corren: `rsi`, `rdx`, `rcx`, `r8`, `r9`, y el sexto en `[rbp+16]`.
- El callee guarda `rdi` en el primer slot de su frame. `return e` copia ahí exactamente el tamaño del valor (un struct `packed` de 3 bytes, byte a byte; un slice, 16 bytes) y deja la dirección en `rax`.
- El destino lo pone el llamador: el slot del `let` (`let p = f()`, `let s: str = f()`); un temporal de su propio frame para una asignación (`p = f()`, `b.p = f()`, `t[i] = f()`, un global), un argumento, `f().x`, `f().len`, `f()[i]`, `print(f())` y una llamada suelta; o su propio destino en `return f()` (no hay copia intermedia).
- Una función que llega al final sin `return` deja ceros en el destino (un struct en cero, un slice con puntero nulo y largo 0), como una función entera devuelve 0. El enum no cambió: sigue sin escribir nada en ese caso.

**Por qué esta y no `rax:rdx` para los 16 bytes de un slice.** SysV devolvería un `{ptr, len}` en `rax:rdx`. Se eligió el puntero oculto porque ya estaba escrito y probado para el enum, porque un solo camino cubre los tres tipos (y cualquier tamaño de struct), porque `return f()` reenvía el destino sin copiar y porque el `let` recibe el valor sin pasar por registros. No se midió ninguna de las dos variantes; no hay una razón medida para cambiar, y este documento no afirma que una sea más rápida.

**Con C.** Para un struct de más de 16 bytes coincide con el retorno en memoria de SysV. Para un struct de 16 bytes o menos y para un slice no coincide (SysV usaría `rax`/`rdx`). Un `extern fn` que devuelve un struct o un slice usa la ABI de Copihue; entre objetos `-c` de Copihue siempre coincide. El compilador no sabe en qué lenguaje está la otra mitad y no da error.

**Modos.** Hosted, `--freestanding`, `--flat` de 64 bits, `-c` y `--nasm` toman el mismo texto. Una función `naked`, `interrupt`, `syscall` o `bits 16`/`bits 32` no puede devolver un struct ni un slice (`a struct cannot be returned from a naked, interrupt, syscall, or bits 16/32 function`, `a slice cannot be returned from ...`), y una `ms64 fn` devuelve en `rax` (`an ms64 function returns an integer or a pointer, not a struct`, `... not a slice`). En hosted `-O2` esa función arma su frame con `rbp` (guarda `rdi`), como la que devuelve un enum, y el local que recibe un struct no va a un registro.

**No se comprueba:** una llamada por puntero a función (`let f = make; f(...)`) a una función que devuelve un struct o un slice no pasa destino en `rdi`; el tipo de un puntero a función no dice qué devuelve. Lo mismo vale desde antes para el enum.

## 2. Devolver un struct

`return` acepta un local, un parámetro, un global, un campo (`b.p`), un elemento (`t[i]`), `*q` con `q: *T`, un literal (`T { ... }`, armado en un temporal en cero) o una llamada que devuelve el mismo struct. Los errores nuevos, todos con `archivo:línea:col`:

| Caso | Mensaje |
|---|---|
| otro struct, aun con los mismos campos | `return value: expected P, found Q` |
| un entero | `return value: expected P, found i64` |
| `return;` | `return needs a 'P' value` |
| `let p: P = f()` con `f` que devuelve `Q` | `expected P, found Q` |
| `p = f()` con `f` que devuelve `Q` | `cannot assign struct 'Q' to struct 'P'` (el de siempre) |
| `f().z` sin ese campo | `struct 'P' has no field 'z'` |
| `n().x` con `n` que devuelve un entero | `'n' does not return a struct; it has no field 'x'` |
| `return p` de un struct desde `-> i64`, `-> *P`, etc. | `return value: expected i64, found P; a struct is returned by value only from '-> P'` |

El último es nuevo y cierra el mismo agujero por el otro lado: la dirección de un struct local usada como valor de retorno escalar también es una dirección del frame muerto.

`let p = make()` sin tipo toma el struct que devuelve la llamada. Sin una llamada, `let x = e` sigue sin inferir un struct.

## 3. Devolver un slice y las vistas a memoria muerta

`-> str` y `-> []T` aceptan en `return` un slice local o global, un parámetro slice, un arreglo global, un literal (`return "cero"`, vive en `.rodata`), un rango (`s[i..j]`), `slice(p, n)`, un campo slice y una llamada que devuelve el mismo slice. Todo menos la llamada se arma con las mismas reglas que `let t: []T = e` y después se copian los 16 bytes. `return 7` es `return value: expected []u8, found i64`; otro elemento, `expected []i64, found []u8`; `return;`, `return needs a '[]u8' value`.

Lo devuelto es una **vista**: un puntero y un largo, sin dueño. Si apunta a memoria del callee, esa memoria se termina con el `return`. El compilador lo detecta, sin costo en el código que sale, en estos casos, y es error:

- `return a` con `a` un arreglo local: `a slice of local array 'a' cannot be returned; the array ends with the function`.
- `return s` con `s` un slice local que vio un arreglo local, un `vec` local (su buffer se libera al salir del bloque) o la dirección de un local, directo o a través de otro slice local, de un rango (`s[0..n]`) o de una asignación (`u = t`): `'s' views memory of this function (a local array, a local vec, or the address of a local) and cannot be returned`.
- `return slice(&x, n)`, `return slice(&a[0], n)`, `return slice(a, n)` con `a` arreglo local, y un rango de un slice marcado: `this slice views memory of this function (...) and cannot be returned`.

La marca es por local y no se borra: si un slice vio memoria del frame y después se le asigna otra cosa, sigue marcado. Eso puede rechazar un programa correcto; no deja pasar uno de esos casos.

**No se detecta** (compila y devuelve memoria muerta):

- Una vista guardada en un campo: `t.text = buf_local; return t.text;` o devolver el struct entero que la contiene. Los campos no llevan la marca.
- Una vista guardada en un global (`g = s_local`) y devuelta después.
- `slice(p, n)` con `p` un puntero local que apunta al frame (`let p: *u8 = &x; return slice(p, 1);`).
- `-> *T` con `return &local` sigue compilando, como antes de 0.45.0 (es un puntero, no un slice).

Un parámetro `s: []T` ve memoria del llamador: `return s` y `return slice(&s[1], 1)` son correctos y no se marcan.

Quien llama recibe la vista en el slot de `let s: str = f()`, en un temporal para `s = f()`, para un argumento, `f().len`, `f().ptr`, `f()[i]` (lectura y escritura, con la misma comprobación de límites que un local en hosted), `print(f())` y `for x in f()`. `f() == s` y `f() == x.f` se comparan (§4). Usarla como escalar es `a slice is not a scalar; read .ptr or .len`; `let s = f()` sin tipo sigue pidiendo el tipo (`a slice must be declared as []u8`).

## 4. `str` / `[]T` como campo de struct

Un campo `text: str` o `items: []Tok` mide 16 bytes, el puntero en el offset 0 y el largo `i64` en el 8, alineado a 8 (a 1 en un `packed struct`). `struct Tok { kind: i64, text: str, line: i32 }` mide 32: `text` en 8, `line` en 24. El elemento sigue las reglas de un slice: entero, puntero o struct. La tabla de tipos lo escribe `[]T` (`str` es `[]u8`).

Entra, en un local, un parámetro, un global, un elemento de arreglo o de slice, un campo de otro struct, a través de `p.f` y `p->f` con `p: *T`, y en el struct que devuelve una llamada:

- `x.f.len`, `x.f.ptr`; `x.f[i]` leído y escrito con la comprobación de límites de un local (hosted; `--freestanding`/`--flat` solo con `--checked`); `x.f[i].y` si el elemento es un struct.
- `x.f = e` con `e` cualquier cosa que acepte `let t: []T = e` (un slice, un arreglo, un literal, un rango, `slice()`, otro campo, una llamada); el literal de struct `T { text: s }` igual.
- `let t: str = x.f`, pasar `x.f` a un parámetro `str`, `return x.f`, `print(x.f)`, `for c in x.f`.
- `==` y `!=` entre un campo `str`/`[]u8` y un local, otro campo, o una llamada que devuelve `str`. Dos nombres siguen siendo el `if`/`for` de antes del frame (`lower_str`); con un campo o una llamada no hay nombre, así que la comparación pasa a ser una llamada a `__slice_eq`, una rutina de 17 instrucciones que el compilador emite una vez y solo si el programa la usa: compara el largo y después los bytes, y sale en la primera diferencia, igual que el `for`. No es de tiempo constante (ver R6 en DESIGN_SECURITY).
- Copiar el struct entero (`v = u`, `let v: Tok = u`, por valor a un parámetro, como valor de retorno): son bytes; el campo copia la vista, no el texto.
- `sizeof` y `offsetof`.

Errores: usar el campo como escalar (`x.f + 1`) es `a slice is not a scalar; read .ptr or .len`; `x.f.len = 3`, `slice field 'len' is read-only`; `x.f.cap`, `slice has no field 'cap'`; `volatile s: str`, `volatile cannot apply to a slice`; `o: []Option`, `a slice element cannot be an enum`; `x.f = 5`, `a slice is initialized from an array, a slice, or slice()`.

**`==` entre dos structs que tienen un campo `str`.** No cambió (eso es SEC-1): compila y compara las **direcciones** de los dos structs, no su contenido ni el texto de sus campos ([DESIGN_SECURITY.md](DESIGN_SECURITY.md) §2.6). `t == u` es 0 aunque `t.text` y `u.text` tengan los mismos bytes. Para comparar el texto se escribe `t.text == u.text`.

## 5. Qué queda fuera (S0 §3.2), con sus mensajes de siempre

- `vec T` como campo, parámetro, retorno o global (`a vec must be a local`, `a vec cannot be returned`).
- `str` en un enum (`an enum field must be an integer or a pointer`).
- `[str; N]`, también como campo (`a slice cannot be an array element`); `vec str`.
- `append`, capacidad, UTF-8, formato.
- Argumentos de línea de comandos.
- `sizeof(T)` dentro de una plantilla.
- Genéricos sobre slices: no se trabajó en ellos. Una sonda con `struct Box<T> { s: []T }` y `Box<i64>` compiló; no hay prueba en la suite.

## 6. Pruebas y qué se verificó dónde

Programas (golden escrito a mano, no ejecutados aquí; los corre `tests/run_tests.sh` en Linux): después se ejecutaron en Linux (Debian, gcc 14.2) y los cuatro goldens coinciden a `-O0` y `-O2` con `tests/run_tests.sh`, `--nasm` y `--native`.

- `tests/test_struct_return.lap`: golden corregido a 10, 20, 99, 88.
- `tests/lang/struct_return.lap`: struct chico (`i32`, `i32`) y grande (6 campos de 1, 2, 4, 8 bytes con relleno, 32 bytes), seis argumentos (el sexto por la pila), llamadas anidadas (`add(add(...), add(...))`), recursión (`fibp(10)` = 55, 89), el resultado usado dos veces y después de una función que escribe 64 qwords en su frame, asignación a local, campo, elemento y global, `f().y`, argumento, todas las formas de `return`, caer al final (ceros) y un `packed struct` de 3 bytes en el elemento del medio de un arreglo sin tocar a los vecinos.
- `tests/lang/slice_return.lap`: literales, una vista del texto del llamador, rangos, un arreglo global, `slice(p, n)` sobre un arreglo del llamador (y que la vista ve el cambio posterior), reenvío, seis argumentos, caer al final (largo 0), `f().len`, `f()[0]`, `print(f())`, argumento, asignación, `==` con otro `str`.
- `tests/lang/struct_str.lap`: todo lo de §4, incluidos `==`/`!=` campo-local, campo-campo, llamada-local y campo de una llamada, un `[]i64` que ve un arreglo global y escribe en él, un `packed struct`, un `[]Tok` de structs que tienen `str`, y escribir a través de `l.items[0].kind`.

Errores (`.err` generados por el compilador y revisados): `tests/errors/sret_*.lap` (struct: tipo, otro struct, `return;`, struct desde `-> i64`, naked, ms64, campo de llamada, `let`; slice: arreglo local, vistas, `&x`, tipos, usos), `slice_ret_vec.lap`, `slice_field.lap`, y `str_ret.lap` y `slice.lap` actualizados (el primero ahora es `return value: expected []u8, found i64`; en el segundo el campo `[]i64` pasó a `[str; 2]`, que sigue siendo error).

**Lo que sí se ejecutó en esta máquina:** `tests/sret/kernel.lap` (30 comprobaciones de retorno de struct y de slice) y `tests/sret/kernel_field.lap` (14 de campos, incluido `__slice_eq` de verdad) arrancan en QEMU (`qemu-system-x86_64`, `--flat 0x8000`, desde el sector de arranque de `tests/usermode/boot.lap`) a `-O0` y `-O2` e imprimen `SRET` por COM1. Es el generador de código freestanding, que comparte con hosted la ABI, las copias y los temporales; las optimizaciones de hosted `-O2` (registros para locales, frame corto) no se ejecutaron, solo se revisaron en el asm. Control negativo: el compilador de 0.44.8 no compila el kernel completo (asignar el resultado era error); una versión recortada (solo `let p = mk(10, 20)` y lecturas) imprime `FAIL01` con 0.44.8 y `SRET` con 0.45.0.

`tests/sret/run.sh` (sección 4l de `tests/run_tests.sh`) además rechaza cada `tests/errors/sret_*.lap` en nativo `-O0`/`-O2`, `--nasm`, `-c`, `--freestanding` y `--flat` con el mismo primer mensaje y línea, sin dejar archivos, y construye los programas de `tests/sret/progs.txt` en nativo, `-c` y `-S` (que `nasm` acepta).

**Texto `-S` de lo demás.** Con `-O0` y `-O2`, los 620 pares programa/nivel de `tests/` y `examples/` que compilan con el compilador de `main` y con este dan el mismo texto salvo `tests/test_struct_return.lap` (que es la Parte 1). El corpus de S0 pasa de 236 a 239 programas (`tests/lang/struct_return.lap`, `slice_return.lap`, `struct_str.lap`).

## 7. Hallazgos que no se corrigieron

Se vieron durante S1; ninguno se tocó (son de antes y no son parte de la lista).

1. **Un literal de arreglo de `u8` local se guarda con paso 8 y se lee con paso 1.** `let b: [u8; 3] = [97, 98, 99];` escribe un qword por elemento (`mov [rbp-24], rax`, `[rbp-16]`, `[rbp-8]`), pero `b[1]` lee `byte [rbp-24+1]`: 0, no 98. Un `str` armado sobre ese arreglo ve `a\0\0`. `tests/lang/str.lap` lo usa (`let raw: [u8; 2] = [104, 105]`) y no lo nota porque compara dos arreglos armados igual. Llenar elemento por elemento (`b[1] = 98`) es correcto.
2. **Un literal de struct como argumento escribe sobre los últimos locales.** `sum(P { x: 3, y: 4 })` guarda 3 y 4 en el slot del último local declarado y en el siguiente (`gen_expr` de `AST_STRUCT_LITERAL` usa el último símbolo como destino). En una sonda pisó un `let keep: i64 = 77` y un campo de otro struct, sin error.
3. **Un campo struct pasado por valor pasa su contenido como dirección.** `sum(r.a)` con `a: P` carga el primer qword de `r.a` y lo pasa como la dirección del struct. Pasar un local (`sum(p)`) es correcto.
4. **`x.y` sobre algo que no es struct no emite nada.** `let x = 5; print_int(x.y);` compila y `print_int` recibe lo que haya en `rax`. Era la otra mitad de la basura de `test_struct_return.lap` (`p1` no era un struct). Con S1, una llamada que no devuelve struct sí da error (§2); el caso general queda.
5. **Una función llamada como una palabra de NASM** (`word`, `byte`, `dword`...) rompe el camino que pasa por `nasm` (`--flat`, `--boot`, `--nasm`): `call word` no ensambla. El nombre no se decora.
6. **`-O0` y `-O2` difieren al tipar `0 - 3` como argumento `i16`:** a `-O0` es `expected i16, found i64`; a `-O2` el plegado lo vuelve literal y compila.
7. **Un struct de tamaño impar desalinea los slots siguientes** del frame (`[rbp-441]` después de un `packed struct` de 17 bytes). Funciona en x86; no se midió si cuesta.
8. `gcc -O2 -Wall -Werror` compila el bootstrap sin advertencias con gcc 14.2 (Debian) y con el clang de Apple.
9. **`&a[i]` de un arreglo local da `Array bounds error` en hosted.** La comprobación de límites de `&arr[i]` compara contra 0, no contra el largo del arreglo: `let arr: [i64; 3]; let p: *i64 = &arr[1];` termina con `Array bounds error` a `-O0` y a `-O2`, también en 0.44.8. Con un arreglo global (`examples/binary_search.lap`) y con `&arr` no pasa. `tests/lang/slice_return.lap` usa `&arr` por eso.

Siguen abiertos de antes: el segfault de `p->campo[i]` del bootstrap ([SELFHOST_S0.md](SELFHOST_S0.md) §3.4; `p->text[0]` con un campo `str` no lo toca: va por el camino nuevo), SEC-1 y SEC-2 de [DESIGN_SECURITY.md](DESIGN_SECURITY.md).
