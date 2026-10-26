# Errores con Result y `?` (corte 0.42.0)

No hay excepciones, ni pila de desenrollado, ni libc. El error es un `enum` que el programa escribe, y `?` es el `match` que ya existía. El layout y la copia genérica están en [`DESIGN_ENUMS.md`](DESIGN_ENUMS.md) y [`DESIGN_GENERICS.md`](DESIGN_GENERICS.md). `vec` interno no se toca.

```copihue
enum Result<T, E> { Ok(T), Err(E) }

fn add1(a: Result<i64, i64>) -> Result<i64, i64> {
    let x = a?;
    return Result<i64, i64>::Ok(x + 1);
}
```

`Result` no es un tipo del compilador. `Ok` y `Err` tienen que ser variantes de un campo. El campo, ya sustituido, sigue siendo un entero o un puntero: un `Result<Result<i64, i64>, i64>` no entra, porque un campo de enum no puede ser un enum.

## `?`

Es postfijo, más fuerte que `+` y que `*`. En este parser el prefijo `*` y `&` envuelven al postfijo, así que `*p?` es `*(p?)`. `a??` es `(a?)?`.

Antes de elegir el frame, `expr?` queda como el `match` que se escribe a mano:

```copihue
match expr {
    Ok(v) => v,
    Err(e) => {
        return Result<i64, i64>::Err(e);
    }
}
```

El orden es `Ok` y después `Err`. Los nombres de las ligaduras no salen en las instrucciones. Si `expr` no es un lugar (`a`, `s.o`, `p->o`), se inserta un `let` justo antes de la sentencia y el `match` usa ese local. `match` de una llamada, escrito a mano, sigue pidiendo un local. Una condición de `while` no se iza: la llamada correría una sola vez, y el error es `'?' in a while condition needs a local Result`.

El brazo que retorna no produce valor. El tipo del `match` es el del brazo `Ok`. Por eso `let q = p?` con `Result<*u8, i64>` deja `q` como `*u8`. Un bloque de brazo que no retorna y no termina en expresión sigue siendo `match arm block needs a result`.

`?` no mira el nombre `Result`. Mira que el operando tenga `Ok` y `Err` de un campo, y que el `Err` de la función tenga el mismo tipo de campo. No hay `From`. Un `Option` es `'?' needs variants Ok(T) and Err(E) on 'Option'`. Sin enum de retorno es `'?' needs a Result return`. Un local que no es enum es `'?' needs a local Result`. `Err` de `*u8` contra un retorno `i64` es `error type '*u8' does not match 'i64' in '?'`. El retorno temprano usa el puntero oculto en `rdi` que ya devolvía un enum. En `bits 16`, `bits 32` y `naked` sigue siendo `an enum is a 64-bit value`.

`match` de las dos variantes sigue siendo obligatorio. Faltar `Err` es `match does not cover 'Err'`.

## Bytes

La copia no agrega instrucciones. En hosted, `let x = a?` y el `match` de arriba, en la misma función `add1` que devuelve `Result<i64, i64>::Ok(x + 1)`, son los mismos bytes. El frame es el de un retorno de enum (`push rbp`), no el frame corto de 0.41: `sub rsp` es `4881EC90000000` (144) en `-O0` y en `-O2`. Listing `nasm -f elf64 -l`, 2026-10-26T10:36:47Z. Las etiquetas `.L0` y `.L1` quedan dentro de la función. El primer `ret` es el retorno de `Err`. El último es el `leave` / `ret` que ya quedaba después del retorno de `Ok`.

`-O0`: 122 y 122 hasta el primer `ret`, 204 y 204 hasta el último. `-O2`: 138 y 138, 221 y 221.

`let x = ok_i(n)?` contra `let t = ok_i(n); let x = match t { ... }` también coincide. El `sub rsp` es `4881ECA0000000` (160). `-O0`: 130 y 130, 212 y 212. `-O2`: 172 y 172, 282 y 282. No hay maquinaria de excepciones.

## Qué no entra

- Excepciones, `panic`, desenrollado, libc, y un `Result` en el heap.
- `From` / `Into`, y un `?` que convierta `E`.
- Bloques `try`, `?` sobre `Option`, y `Ok` o `Err` sin campo o con más de uno.
- Un `Ok` cuyo campo sea un enum.
- Izar un `?` fuera de la condición de un `while`.
- Reescribir `bench/src/parse.lap`, reescribir `vec`, o un `Result` basado en traits.
- Un `if` sobre la etiqueta cruda. No hay otro bajado que el `match`.
