# Enums y `match` (corte 0.38.0)

Un `enum` es un tipo suma. Cada variante tiene una etiqueta entera y cero o más campos. El valor cabe en un local o en un parámetro. No hay heap, no hay libc, y el mismo código sirve en hosted, `--freestanding` y `--flat`.

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

`take(o)` pasa la dirección, como un struct, y el callee copia. `take(Option::Some(7))` llena un temporal del tamaño del enum y pasa esa dirección. Un enum usado como escalar es `an enum is not a scalar; match it`. Indexarlo es `cannot index '...'; match it`.

La aridad se comprueba en el constructor y en el patrón: `variant 'Some' takes 1 field, not 0`. `None()` en un uso es `variant 'None' has no fields; omit the parentheses`. Un nombre que no existe es `enum 'Option' has no variant 'Nope'`. El tipo de un campo es el chequeo que ya había: `expected i64, found *u8`.

## `match`

`match` es una expresión. El brazo también. El sujeto tiene que ser un local o un parámetro cuyo tipo es el enum (`match needs a local enum`). Cada variante entra una vez (`variant 'Some' is already matched`). Un nombre de otro enum es `variant 'Color::Red' is not in 'Option'`. Si falta alguna, un solo diagnóstico las nombra todas y el caret queda en `match`: `match does not cover 'None', 'Other'`.

El código es `cmp qword [rbp+off], tag` / `jne` al brazo siguiente. El payload se carga en slots nuevos del brazo (signo o cero según el campo). El cuerpo deja un escalar en `rax`. Al final de los brazos hay un `ud2`: una etiqueta corrupta no sigue. Un programa exhaustivo no lo ejecuta. No hay `syscall`.

Los brazos se unifican por el entero más ancho. Un literal cuenta como `i64`, así que el orden no cambia el tipo y un `match` de solo literales es `i64` (`let x: i8 = match ... => 1` es `expected i8, found i64`). Un puntero tiene que ser compatible con el otro; si uno es `volatile` y el otro no, gana el volátil cuando la asignación ya lo permitía. Mezclar puntero y entero es `match arm: expected ..., found ...`. Un brazo que es otro enum es `an enum is not a scalar; match it`.

## Qué no entra

- Devolver un enum. `rax` no alcanza para etiqueta y payload, y un struct que se declara `-> T` ya vuelve solo en `rax`. `-> Option` y `return o` son `an enum cannot be returned`.
- Un global (`an enum must be a local or a parameter`), un campo de struct (`an enum cannot be a struct field`), un elemento de slice, de array o de `vec`.
- Un enum dentro de un `module`. `pub enum` fuera de un módulo es el `pub is only valid inside a module` que ya existía.
- `bits 16`, `bits 32` y `naked`, incluido el naked de 64 bits: `an enum is a 64-bit value`.
- `let o: Option;` sin valor (`an enum local needs a value`). `let volatile` sobre un enum (`volatile cannot apply to an enum`).
- `match` de un puntero, de `*p`, o de un constructor que no está en un local. `&o` es la dirección de los bytes; no se puede hacer `match` a través de ella.
- `_`, patrones con `|`, guardas, brazos que son bloques, campos con nombre (`Some { x: i64 }`) y un enum anidado como campo.
- Liberar un `own *T` guardado en una variante. Ese campo se rechaza al parsear. Reasignar el enum sí copia los bytes: no es un dueño.

El registro de `-O2` no mete el enum en un registro, ni un parámetro enum. La función arma el frame de `rbp`. El frame corto de `push`/`pop` no tiene `rbp`, y copiar la etiqueta ahí pisaba el frame del llamador: `take` de un `Option` que no era el primer local devolvía el valor equivocado. `match` también cuenta como uso de stack, así que no entra en ese frame corto. `--freestanding` y `--flat` no usan ese plan, así que el flujo de `tests/freestanding/enum.lap` y la imagen de `tests/flat/enum.lap` coinciden en `-O0` y `-O2`. `tests/flat/enum.lap` no se arrancó en QEMU.
