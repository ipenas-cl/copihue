# Slices y nivel fácil (v0.34.0)

**Corte 0.43.0.** `str` es este `[]u8`. El detalle está en [`DESIGN_STRINGS.md`](DESIGN_STRINGS.md).

El nivel fácil es el que decide el compilador. El nivel explícito es el que decide quien escribe. Un slice es un puntero y un largo, no un tercer array. `[T; N]`, el decaimiento a `*T` y la comprobación de un array local en hosted no cambian. Indexar un puntero sigue sin comprobación.

## Representación

`[]T` es 16 bytes: el puntero en el offset 0 y el largo `i64` en el offset 8. `T` es un entero, un puntero o un struct, lo mismo que admite `[T; N]`. `sizeof` de un slice es 16.

```copihue
let buf: [i64; 4] = [10, 20, 30, 40];
let s: []i64 = buf;          // puntero al primer elemento, largo 4
let n: i64 = s.len;          // 4
let p: *i64 = s.ptr;         // el puntero, sin largo
```

- `[]T` se escribe con corchetes vacíos. `[T; N]` sigue siendo el array. Un elemento que es un slice es `a slice cannot be an array element`. Un campo que es un slice es `a slice cannot be a struct field`. Un `-> []T` es `a slice cannot be returned`.
- En `bits 16` y `bits 32` es `[]T is a 64-bit value`.
- `s.ptr` es `*T` y `s.len` es `i64`. Los dos se leen. Asignarlos es `slice field 'ptr' is read-only` (o `'len'`).
- `let s: []T = arr` convierte un `[T; N]` local o global del mismo elemento. Otro slice se copia (puntero y largo). `let s: []T = slice(p, n)` lo arma desde un `*T` y un largo: el puntero no se comprueba. Un literal negativo es `negative slice length`. En hosted, y con `--checked`, un largo negativo que no es literal escribe `negative slice length` y sale con código 1. En `--freestanding`, `--flat` y `--boot` sin `--checked` ese largo se guarda tal cual.
- Un elemento que no coincide es `expected []i64, found [u8; 4]`. Otra inicialización es `a slice is initialized from an array, a slice, or slice()`.
- Un parámetro `s: []T` viaja como la dirección de esos 16 bytes. El callee los copia, igual que un struct por valor. El llamador pasa un slice, no el array pelado: la conversión es el `let`.
- `let s: []T;` sin inicializador es puntero nulo y largo 0.
- `s = t` copia los 16 bytes.

## Indexación

`s[i]` comprueba `i` como entero sin signo contra el largo. Un índice negativo no pasa. Si falla, el programa escribe `index out of range` y un salto de línea en el fd 2 (syscall 1) y termina con el syscall 60 y código 1. No hay libc.

Eso es el nivel fácil, y es el default de un programa hosted. `--freestanding`, `--flat` y `--boot` no insertan esa comprobación. `--checked` la pide en esos modos. No enciende la comprobación de los arrays locales: esa sigue siendo solo hosted, como hasta ahora.

Sin comprobación, a propósito:

- `unsafe { ... }` es un bloque. Dentro, `s[i]` y el array local hosted no comprueban. Los tipos sí. `unsafe` solo abre el bloque cuando el token siguiente es `{`.
- `s.ptr[i]` es un puntero. Nunca comprueba, en hosted y en el kernel.

`s[i] = v` usa la misma regla. Si `T` es un struct, `s[i]` es la dirección del elemento, como en un array, y `s[i].field` escribe ese campo. `for x in` de un elemento struct es `for in of a struct element; index the slice`.

## `for`

`for (init; cond; step)` no cambia. `in` y `..` son contextuales. `..` es un token.

```copihue
for x in s { t = t + x; }     // s es []T o [T; N]
for i in 0..s.len { t = t + s[i]; }
for i in 0..4 { t = t + a[i]; }
```

- `for x in e` exige un slice o un array. Si no, `for in expects a slice or an array`. El compilador baja a un `while` de `0` hasta el largo y carga el elemento sin comprobación: el índice es el del bucle y solo crece en uno.
- `for i in inicio..fin` es un `while` desde `inicio` hasta `fin`, sin incluir `fin`. `i` es `i64`.
- La comprobación de `s[i]` se omite cuando `inicio` es el literal `0`, `fin` es `s.len`, y el cuerpo no asigna `i`. `a[i]` en `for i in 0..N` se omite cuando `N` es un literal y el array tiene al menos `N` elementos, y el cuerpo no asigna `i`. Otro índice, otro array más corto, o una asignación a `i`, mantiene la comprobación. Vale en `-O0` y en `-O2`. No depende del probador de `while` de hosted `-O2`, que sigue igual.

## Qué no entra

No hay capacidad, ni `append`, ni slice de slice como valor anidado en un struct, ni devolver un slice, ni comprobación de un puntero que no sea un slice. El costo de la comprobación se mide fuera de `make test`, en `bench/slice/`: el mismo `while` con `s[i]` y con `s.ptr[i]`, en rangos.
