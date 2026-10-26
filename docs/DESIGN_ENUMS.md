# Enums y `match` (corte 0.39.0)

**Corte 0.41.0.** `enum Option<T> { None, Some(T) }` cuando el campo, ya sustituido, es un entero o un puntero. `enum Option { None, Some(i64) }` no cambia. El detalle está en [`DESIGN_TRAITS.md`](DESIGN_TRAITS.md).

Un `enum` es un tipo suma. Cada variante tiene una etiqueta entera y cero o más campos. El valor cabe en un local, un parámetro, un global o un campo de struct. No hay heap, no hay libc, y el mismo código sirve en hosted, `--freestanding` y `--flat`.

`enum` y `match` no son palabras clave. `let enum = 1` sigue siendo un nombre. `match(1)` sin una `{` después es una llamada. `match e { ... }` es la expresión.

`Option` no es un tipo del compilador. El programa lo escribe:

```copihue
enum Option { None, Some(i64) }
```

## Layout

La etiqueta es un `i64` en el offset 0. El payload empieza en 8. Los tags son el orden del fuente, desde 0. Una variante repetida no entra en esa cuenta (`variant 'Some' is already defined`).

Cada campo es un entero (`i8` `u8` `i16` `u16` `i32` `u32` `i64` `u64`) o un puntero (`*T`, también `*volatile T`). El campo se alinea a su ancho y el valor se redondea a 8. `Option` mide 16. `enum Color { Red, Green, Blue }` mide 8. `enum Pair { Empty, Both(i64, i32), Byte(u8) }` mide 24: el `i64` en 8, el `i32` en 16. Tope: 16 campos, o `variant '...' has too many fields`. Payload por encima de 4096: `variant '...' is too large`.

Un campo que no es entero ni puntero es `an enum field must be an integer or a pointer`. `None()` en la definición es `variant 'None' has no fields; omit the parentheses`.

## Construcción y copia

`Option::Some(7)` y `Option::None` escriben en un local: el slot se pone en cero, luego la etiqueta y los campos. Un campo angosto se trunca al guardar. `let x = Option::Some(1)` y `let x: Option = Option::Some(1)` toman el tipo del enum. `x = Option::None` y `x = y` del mismo enum copian los bytes. Otro enum es `expected Option, found Color`.

`take(o)` pasa la dirección, como un struct, y el callee copia. El parámetro es una copia: `clobber` puede asignar `Option::Some(9)` y el local del llamador sigue siendo el valor de antes. Pasar el mismo enum dos veces es otra copia, no un movimiento. `take(Option::Some(7))` llena un temporal del tamaño del enum y pasa esa dirección. Lo mismo vale para un global, un campo y el resultado de una llamada que devuelve el enum: la llamada escribe un temporal y se pasa esa dirección. Un enum usado como escalar es `an enum is not a scalar; match it`. Indexarlo es `cannot index '...'; match it`.

La aridad se comprueba en el constructor y en el patrón: `variant 'Some' takes 1 field, not 0`. `None()` en un uso es `variant 'None' has no fields; omit the parentheses`. Un nombre que no existe es `enum 'Option' has no variant 'Nope'`. El tipo de un campo es el chequeo que ya había: `expected i64, found *u8`.

## Devolver un enum

No se usa `rax:rdx`. `Color` mide 8, `Option` mide 16 y `Pair` mide 24. Un solo ABI cubre los tres: el llamador pasa en `rdi` un puntero al destino, como el argumento oculto de SysV para un struct grande. El callee guarda ese puntero en un slot anónimo del frame (`mov [rbp-8], rdi`, sin un símbolo `__eret`) y escribe los bytes a través de él. Después de los drops, `rax` queda con esa dirección. Quien usa el valor lee el slot, no `rax`.

Los parámetros reales se corren. Los cinco primeros van en `rsi`, `rdx`, `rcx`, `r8` y `r9`. El sexto es el primer slot de stack, en `[rbp+16]`. Una función que devuelve un enum tiene cinco parámetros en registro, no seis. `fn make(n: i64) -> Option` recibe `n` en `rsi`: `make(4)` imprime 4, no el puntero. `make5` suma cinco registros. `make6` suma el primero con el sexto, que está en `[rbp+16]`.

`let x = give()` y `let x: Option = give()` ponen el destino en el slot de `x`. `return give()` reenvía el puntero oculto del caller. `give();` descartado reserva un temporal para que el callee tenga dónde escribir. `return o` y `return Option::Some(n)` copian al destino. Otro enum es `expected Option, found Color`. Devolver un enum desde `-> i64` sigue siendo `an enum cannot be returned`, con el caret en `return`.

Una llamada indirecta no tiene este ABI: `an indirect call cannot return an enum`. `bits 16`, `bits 32`, `naked`, `interrupt fn` y `syscall fn` tampoco: en la definición es `an enum cannot be returned`, y un valor enum en el cuerpo es `an enum is a 64-bit value`. Hosted, `--freestanding` y `--flat` de 64 bits sí pueden devolver un enum. El frame corto de `push`/`pop` no llama a una función que devuelve un enum, porque el destino vive en `[rbp]`.

## `match`

`match` es una expresión. El brazo es una expresión o un bloque. El sujeto es un local, un parámetro, un global o un campo cuyo tipo es el enum. `s.o` y `p->o` (un campo de un struct a través de un puntero) entran. `match Option::Some(1)`, `match give()` y `match *p` de un puntero a enum siguen en `match needs a local enum`. `&o` es la dirección de los bytes; no se hace `match` a través de ella.

Cada variante entra una vez (`variant 'Some' is already matched`). Un nombre de otro enum es `variant 'Color::Red' is not in 'Option'`. Si falta alguna, un solo diagnóstico las nombra todas y el caret queda en `match`: `match does not cover 'None', 'Other'`.

`_` cubre lo que falte y no emite `ud2`. `Option::_` es `'_' is not a variant` y no cubre. `_(x)` o `_()` es `'_' has no fields` y tampoco cubre. Un segundo `_` es `'_' is already matched`. Un brazo después de un `_` bien formado es `arm after '_' is unreachable`.

Un bloque después de `=>` es el cuerpo del brazo. La última sentencia tiene que ser una expresión (`n + k;`). Si el bloque está vacío, o la última sentencia es un `let`, un `if` o algo que no deja valor, el diagnóstico es `match arm block needs a result` y el caret está en `{`.

En `-O0`, en `--freestanding` y en `--flat`, el código de un local sigue siendo `cmp qword [rbp+off], tag` / `jne` al brazo siguiente. En hosted `-O2` la etiqueta se carga una vez (`mov rbx, [slot]`) y cada brazo hace `cmp rbx, imm` / `jne`. El cuerpo puede pisar `rbx`; ese camino salta al final, así que el `cmp` siguiente solo se alcanza por el `jne`, con `rbx` intacto. Un global es `mov rbx, [nombre]`. Un campo a través de puntero es `mov rax, [rbp+slot]` y `mov rbx, [rax+off]`, una sola vez. El payload se sigue volcando a un slot nuevo (`mov [slot], rax` / `mov rax, [slot]`). Ese vuelco no se quitó.

El payload se carga en slots nuevos del brazo (signo o cero según el campo). El cuerpo deja un escalar en `rax`. Si no hay `_` y ninguna etiqueta coincide, hay un `ud2`. Un programa exhaustivo no lo ejecuta. No hay `syscall`.

Los brazos se unifican por el entero más ancho. Un literal cuenta como `i64`, así que el orden no cambia el tipo y un `match` de solo literales es `i64` (`let x: i8 = match ... => 1` es `expected i8, found i64`). Un puntero tiene que ser compatible con el otro; si uno es `volatile` y el otro no, gana el volátil cuando la asignación ya lo permitía. Mezclar puntero y entero es `match arm: expected ..., found ...`. Un brazo que es un enum, un struct o un slice es `an enum is not a scalar; match it`.

## Global y campo

`let g: Option;` es BSS: `resb` del tamaño, etiqueta 0. Si `None` es la primera variante, ese cero es `None`. Un inicializador (`let g: Option = Option::None`) es `an enum global starts at zero; assign it in a function`. `let volatile g: Option` es `volatile cannot apply to an enum`. La asignación `g = Option::Some(1)` y `g = local` se escribe dentro de una función. `match g` lee esa etiqueta.

`struct Box { o: Option, n: i64 }` es legal. El campo se alinea a 8 y `sizeof` del enum es el ancho del campo. `b.o = Option::Some(12)`, `let c = b.o`, `match b.o` y `fn from_box(b: Box) -> Option { return b.o; }` copian los bytes. `let p: *Box = &b` permite `match p->o` y `p->o = Option::None`. Un array de enum (`a: [Option; 2]`) sigue siendo `an enum cannot be an array element`. Un elemento de slice o de `vec` también.

## Frame

El registro de `-O2` no mete el enum en un registro, ni un parámetro enum, ni un retorno enum. La función arma el frame de `rbp`. El frame corto de `push`/`pop` no tiene `rbp`, y copiar la etiqueta ahí pisaba el frame del llamador: `take` de un `Option` que no era el primer local devolvía el valor equivocado. `match` también cuenta como uso de stack, así que no entra en ese frame corto. Una función que devuelve un enum tampoco: tiene que guardar `rdi`. `--freestanding` y `--flat` no usan ese plan, así que el flujo de `tests/freestanding/enum.lap` y la imagen de `tests/flat/enum.lap` coinciden en `-O0` y `-O2`. `tests/flat/enum.lap` no se arrancó en QEMU. Esos dos programas devuelven `i64`, no el enum; el puntero oculto no entra en su flujo.

## Qué no entra

- `match` de un constructor, de una llamada o de `*p` cuando `p` es un puntero al enum (`match needs a local enum`).
- Patrones con `|`, guardas, campos con nombre (`Some { x: i64 }`) y un enum anidado como campo de otro enum.
- Un enum dentro de un `module`. `pub enum` fuera de un módulo es el `pub is only valid inside a module` que ya existía.
- `bits 16`, `bits 32` y `naked`, incluido el naked de 64 bits: `an enum is a 64-bit value`. Tampoco `interrupt fn` ni `syscall fn` pueden devolver un enum.
- `let o: Option;` sin valor (`an enum local needs a value`). Un global sin inicializador sí: arranca en cero.
- Un elemento de slice, de array o de `vec`.
- Liberar un `own *T` guardado en una variante. Ese campo se rechaza al parsear. Reasignar el enum sí copia los bytes: no es un dueño.
- Una llamada indirecta que devuelve un enum.
- Quitar el vuelco del payload a un slot. La etiqueta en `rbx` no alcanza para eso, y el `-O0` no cambia.
