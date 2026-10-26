# Genéricos por monomorfización (corte 0.40.0)

Una función o un struct puede llevar parámetros de tipo. El compilador genera una copia por cada combinación que el programa usa. Esa copia es una función o un struct normal: en ejecución no hay un diccionario ni una búsqueda. `vec` y `Option` siguen como están; este corte es lo que haría falta para escribirlos en el lenguaje más adelante, y no los reescribe.

```copihue
fn max<T>(a: T, b: T) -> T {
    if (a > b) { return a; }
    return b;
}

struct Par<A, B> { a: A, b: B }

fn main() -> i64 {
    let a: i32 = 3;
    let b: i32 = 9;
    print_int(max<i32>(a, b));   // explícito
    print_int(max(a, b));        // T sale de los argumentos
    let p = Par<i32, i64> { a: 4, b: 5 };
    return 0;
}
```

No hay traits. La operación no se comprueba en la plantilla. Se comprueba al generar la copia, y el error nombra el tipo y la operación: `cannot apply '+' to 'Point' in 'add<Point>'`.

## Qué puede ser un argumento de tipo

Un entero (`i8` `u8` `i16` `u16` `i32` `u32` `i64` `u64`), un puntero (`*T`, `*mut T`, `*volatile T`, y punteros anidados) o un struct, incluido un struct genérico ya instanciado (`Box<Par<i32, u8>>`).

No entra un enum, un slice, un array, un `vec` ni un `own *T`: `a type argument cannot be a slice, an array, a vec, or an owning pointer`, o `type argument 'Option' is not an integer, a pointer, or a struct`. Un nombre que no existe es `unknown type argument 'Foo'`.

`mut` no es parte del tipo, así que no es parte del símbolo: `*mut i32` y `*i32` son la misma copia. `*volatile i32` es otra (`pv$`).

Un parámetro de función puede ser `[]T`. `T` sale del elemento del slice. Eso no convierte al slice en un argumento de tipo: `id<[]i32>` se rechaza. Un campo `[T; N]` no está probado y no se promete.

Hasta 8 parámetros. Más es `too many type parameters`. Repetir el nombre es `type parameter 'T' is already defined`. Usar un entero del lenguaje como nombre es `type parameter 'i32' hides a builtin type`. `<>` vacío es `expected a type parameter`.

## Nombres

El símbolo es estable y no depende de `mut`. La gramática es un entero, `p$` o `pv$` más el apuntado, o `Nombre$cantidad$args`. Un dígito después de `$` es la cantidad, no un apuntado: el struct `p` con un argumento es `p$1$i32`, y `*u8` es `p$u8`. `$` no es un carácter de identificador en el fuente, así que el programador no choca con ese nombre.

| Fuente | Símbolo |
|---|---|
| `max<i32>` | `max$1$i32` |
| `id<*u8>` | `id$1$p$u8` |
| `Par<i32, *u8>` | `Par$2$i32$p$u8` |
| `Box<Par<i32, u8>>` | `Box$1$Par$2$i32$u8` |
| `mem::max<i32>` | `mem__max$1$i32` |

Dentro de un módulo, `link_sym` envuelve el nombre ya mangled. La plantilla no se exporta y no entra en la tabla de tipos. La copia sí: en `-c` el objeto que la define emite `global max$1$i32`, y el que la declara con `extern fn max<T>` emite `extern max$1$i32`. `pub` sigue siendo visibilidad de fuente, no de ELF. Dos objetos que emiten el mismo cuerpo siguen chocando en `ld`: no hay COMDAT ni símbolo débil.

`>>` dentro de los argumentos de tipo son dos `>`. `x >> 2` sigue siendo un desplazamiento. `a < b` sigue siendo una comparación: `<...>` solo es una lista de tipos si un `>` la cierra y lo que sigue es `(` o `{`.

## Inferencia y forma explícita

Si la llamada no escribe los argumentos de tipo, cada parámetro se lee de los argumentos de valor cuyo tipo lo menciona. Un `T` a secas se queda con el tipo entero del argumento, puntero incluido. `*T` no unifica con `*volatile u8` (`cannot infer type argument 'T' for 'star' from '*volatile u8'`). Dos argumentos que no coinciden son `type argument 'T' is 'i32' and 'i64' in 'max'`. Un literal es `i64`, así que `max(1, 2)` es `max<i64>`. Si no hay de dónde sacarlo, `cannot infer type argument 'T' for 'make'`.

`max<i32>(1, 2)` fija el tipo y después corre el chequeo de argumentos que ya había. Un literal que cabe en `i32` pasa. La cantidad mal puesta es `function 'max' takes 1 type argument, not 2`. Un builtin o una función no genérica con argumentos de tipo es `function 'print_int' takes 0 type arguments, not 1`.

Un struct se nombra con sus argumentos: `Par<i32, i64>`, `let p: Par<i32, i64>`, `Par<i32, i64> { a: 4, b: 5 }`. Sin ellos es `struct 'Par' needs type arguments`. Otra cantidad es `struct 'Par' takes 2 type arguments, not 1`. `sizeof(Par<i32, i64>)` y `offsetof` usan el nombre ya mangled. Un literal `Par { ... }` sin argumentos no se infiere.

## Copias

La plantilla se parsea y no se genera. Cada combinación usada se clona, se sustituye y se emite después. La llamada queda apuntando al símbolo de la copia, así que una segunda pasada no la vuelve a clonar. Una llamada recursiva encuentra la copia ya registrada y no genera otra. El tope es 64 structs y 64 funciones (`too many monomorphized copies`).

Los structs se instancian antes del layout, de adentro hacia afuera, para que un campo `Par<i32, i64>` ya tenga tamaño. `*Node<T>` dentro de `Node<T>` es un puntero: no hace falta el layout del struct para medirlo, y no se entra en un ciclo. La copia conserva `bits` y `naked` de la plantilla, así que un genérico de enteros en `bits 16` o `bits 32` es la función que el compilador ya emitía para ese ancho. Eso se ensambló y no está en la suite; no se arrancó.

En `--freestanding` y `--flat` el código es el mismo si no pide `malloc`. `_start` se emite primero. El nombre de la copia lleva `$`, así que no gana el lugar de la entrada.

## Bytes

La copia no agrega instrucciones. En hosted, `max<i32>` y un `max_i32` escrito a mano, con el mismo `if (a > b) { return a; } return b;`, generan los mismos bytes (listado NASM, de la etiqueta hasta el primer `ret`, y también el cuerpo entero). `-O0` son 52 hasta ese `ret` y 65 el cuerpo. `-O2` son 43 y 59. El detalle está en el changelog.

## Qué no entra

- Traits, interfaces, cláusulas `where` y especialización.
- Enums genéricos, métodos genéricos y un genérico usado como valor (`let f = max`).
- Un argumento de tipo que sea enum, slice, array, `vec` u `own`.
- Reescribir `vec` u `Option`. Siguen siendo lo que ya eran.
- Genéricos de constantes, tipos de orden superior y un global genérico.
- COMDAT o un símbolo débil para que dos objetos `-c` emitan el mismo cuerpo.
- Inferir los argumentos de un literal de struct.
- Devolver un struct desde una función genérica: es el mismo camino de `rax`, sin chequeo, que un struct escrito a mano.
- `bits 16` y `bits 32` no tienen una prueba en la suite. El entero cae porque la copia es una función común; no se promete más que eso.
