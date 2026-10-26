# Cadenas `str` (corte 0.43.0)

`str` es un `[]u8`. Son 16 bytes: el puntero en el offset 0 y el largo `i64` en el offset 8. No es una clase, no hay heap y no hay libc. El slice está en [`DESIGN_SLICES.md`](DESIGN_SLICES.md). `str` no es una palabra clave: `let str = 1` sigue siendo un nombre. En posición de tipo, y si no es un parámetro de tipo, es ese slice. `sizeof(str)` es 16.

```copihue
let s: str = "hola";
let n: i64 = s.len;     // 4
let c: i64 = s[0];      // 104
let t: str = s[1..3];   // "ol"
```

## Qué entra

- Un literal `"..."` en un destino `str` o `[]u8` guarda la etiqueta del literal y el largo decodificado, sin contar el NUL que se deja al final para que `.ptr` siga siendo una cadena de C. `let p = "..."` y un destino `*u8` siguen siendo el puntero de hoy. `print("...")` no cambia.
- `s.len` es `i64`. `s.ptr` es `*u8`. Los dos se leen. Asignarlos es el error de campo de slice que ya existe.
- `s[i]` comprueba el índice como entero sin signo contra el largo. Si falla, escribe `index out of range` y sale con código 1. La misma regla que un slice: hosted sí, `--freestanding` / `--flat` / `--boot` no, salvo `--checked`. `unsafe { }` y `s.ptr[i]` no comprueban.
- `a == b` y `a != b` entre dos `str` (o dos `[]u8`) comparan el largo y después los bytes. Antes del frame queda el `if` y el `for i in 0..a.len` que se escribiría a mano. No hay `memcmp`.
- `s[i..j]` es un `str` local: puntero `s.ptr + i` y largo `j - i`. `j` no se incluye. Hace falta `i <= j` y `j <= s.len`, sin signo. Si no, el mismo `index out of range`. No se devuelve: `-> str` es `a slice cannot be returned`.
- `print(s)` y `println(s)`, con `s` un `str`, son `write(1, s.ptr, s.len)` más el salto de línea de `println`. El syscall 1 escribe el largo. No llama a `__strlen` y no usa libc. Un `*u8` sigue pasando por `__strlen`. Solo hosted.

Un parámetro `s: str` viaja como la dirección de los 16 bytes. El callee los copia, igual que `[]u8`.

## Qué no entra

No hay UTF-8, ni `String`, ni formato, ni interpolación, ni capacidad, ni `append`. Un campo de `enum` no puede ser un `str`: `Result` sigue llevando un entero o un puntero, así que un token se cuenta o se describe con un índice, y el texto se queda en el `str`. No se reescribe `vec`. No se cambia `print` de un buffer con NUL, ni `strlen` / `strcpy` / `strcmp`. Comparar un `[]i64` por contenido no entra. `bits 16` y `bits 32` siguen en `[]T is a 64-bit value`.
