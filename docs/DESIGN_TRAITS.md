# Traits por monomorfización (corte 0.41.0)

**Corte 0.42.0.** `Result<T, E>` y `?` no son un trait. Son el enum genérico y un `match`. El detalle está en [`DESIGN_ERRORS.md`](DESIGN_ERRORS.md).

Un trait es una lista de firmas. El compilador no genera código para el trait. Genera la función del `impl` como una función común, y la copia de un genérico la llama por su símbolo. En ejecución no hay tabla virtual, ni diccionario, ni una búsqueda. El detalle de la copia está en [`DESIGN_GENERICS.md`](DESIGN_GENERICS.md). `vec` interno no se toca.

```copihue
trait Ord { fn lt(a: Self, b: Self) -> bool }

impl Ord for i64 {
    fn lt(a: Self, b: Self) -> bool {
        return a < b;
    }
}

fn max<T: Ord>(a: T, b: T) -> T {
    if (lt(a, b)) { return b; }
    return a;
}
```

`trait`, `impl`, `Self` y `bool` no son palabras clave. `let trait = 1` sigue siendo un nombre.

## Despacho

`impl Ord for i64` registra que `i64` implementa `Ord` y emite `fn lt` con el símbolo `lt$Ord$i64`. `Self` en esa firma es el tipo del impl. La plantilla `max<T: Ord>` no se emite. Al usarla con `i64`, la restricción se comprueba y la llamada `lt(a, b)` queda en `call lt$Ord$i64` antes de elegir el frame. Por eso el `-O2` de la copia es el mismo frame corto que el de una función escrita a mano.

`T: Ord + Eq` son dos restricciones. Las dos tienen que cumplirse. Una función del programa que se llame `lt` gana: la llamada, también `a.lt(b)`, es esa función y no el impl.

El trait y el impl viven en el espacio plano. Dentro de un módulo es `a trait must be in the flat namespace` o `an impl must be in the flat namespace`. Un método del trait no tiene cuerpo (`a trait method has no body`) y no es genérico (`a trait method cannot be generic`). Un trait vacío no se registra (`a trait needs a method`). Repetir el trait es `trait 'Ord' is already defined`. Repetir el impl es `type 'i64' already implements 'Ord'` y ese cuerpo no entra. Faltar un método es `impl 'Ord' for 'i64' is missing method 'lt'`. Una firma distinta es `method 'lt' of impl 'Ord' for 'i64' does not match the trait`. Un método de más es `method 'eq' is not in trait 'Ord'`.

El tipo del impl es un entero, un puntero o un struct. Otra cosa es `type 'Foo' is not an integer, a pointer, or a struct`. Un slice, un array, un `vec` o un `own *T` es `an impl type cannot be a slice, an array, a vec, or an owning pointer`.

## La restricción

Se mira al generar la copia, en el uso, no en la plantilla. Si el tipo no implementa el trait, el error nombra los dos: `type 'Point' does not implement 'Ord'`. No se crea la copia. Un nombre que no es un trait es `unknown trait 'Nope'`, una vez, en la plantilla.

## Llamada con punto

`a.lt(b)` es azúcar de `lt(a, b)`. `p->f(a)` es `f(*p, a)`. El `(` es lo que la distingue de un campo: `p.x` sigue siendo el campo. Si no hay una función del programa con ese nombre, una sola coincidencia reescribe la llamada al símbolo del impl. Ninguna, en una llamada con punto, es `no method 'lt' for 'Point'`. Dos es `ambiguous method 'lt' for 'i64'`. Sin el punto, cero coincidencias sigue siendo el error de función desconocida que ya había.

Eso no infiere argumentos de tipo en el método. `x.f::<T>()` no es esta forma. Un método sin parámetros tampoco: la resolución necesita el primer argumento.

## `bool`

`bool` es un entero con signo de 64 bits, el mismo ancho que `i64`. El impl puede devolver `a < b`: la comparación, usada como valor, sigue siendo `i64`, y `if` mira si `rax` no es cero. `print_int` de ese valor imprime 0 o 1. Un campo `bool` mide 8. Usarlo como nombre de parámetro de tipo es `type parameter 'bool' hides a builtin type`.

## `enum Option<T>`

Si el campo, ya sustituido, sigue siendo un entero o un puntero, el enum puede ser genérico. Es una prueba, no una reescritura de cada `Option` que ya había. `enum Option { None, Some(i64) }` no cambia de layout.

```copihue
enum Option<T> { None, Some(T) }

fn wrap<T>(v: T) -> Option<T> {
    return Option<T>::Some(v);
}
```

`Option<i64>::Some(7)` y `Option<i64>::None` son el constructor de la copia `Option$1$i64`. `<...>` también cierra delante de `::`. `sizeof(Option<i64>)` es 16, como el `Option` concreto: etiqueta 8 y payload 8. Se devuelve por el puntero oculto en `rdi` que ya devolvía un enum. Un `let` de esa llamada ve el tipo de la copia. `match` de la llamada sigue pidiendo un local.

Un campo que sustituye a un struct es `an enum field must be an integer or a pointer`, y esa copia no se registra. El nombre pelado es `enum 'Option' needs type arguments`. Otra cantidad es `enum 'Option' takes 1 type argument, not 2`. Un enum sigue sin ser argumento de tipo. El tope de copias de enum es 64, el mismo mensaje `too many monomorphized copies`.

`--freestanding` y `--flat` llaman `max$1$i64` y `lt$Ord$i64`. No hay `syscall` si el programa no lo pide. `_start` se emite primero: el `$` no le gana el lugar. La imagen plana no se arrancó.

## Bytes

La copia no agrega instrucciones. En hosted, `lt$Ord$i64` y un `lt_i64` escrito a mano, los dos `return a < b`, son los mismos bytes. `max$1$i64` y un `max_i64` que llama a esa función tienen el mismo largo y las mismas instrucciones. Lo único que cambia son los cuatro bytes de desplazamiento del `call`: en el archivo genérico `main` queda entre el impl y la copia, y en el otro `max` sale justo después de `lt`. No hay tabla virtual. El detalle está en el changelog.

## Qué no entra

- Métodos con cuerpo en el trait, tipos asociados, `trait Foo<T>`, cláusulas `where`, especialización, objetos de trait y una tabla virtual.
- `Ord::lt`, un método genérico y `x.f::<T>()`.
- Un impl de un slice, un array, un `vec` o un `own *T`.
- Reescribir `vec`, o reescribir cada `Option` que ya estaba en tests y ejemplos.
- Un trait o un impl dentro de un módulo, `pub trait`, y un método sin parámetros.
- Genéricos de constantes, tipos de orden superior, y usar el nombre del trait como valor.
- Un campo de enum que, sustituido, no es entero ni puntero. Un enum como argumento de tipo.
