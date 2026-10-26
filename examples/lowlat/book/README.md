# Libro de órdenes

[book.lap](../book.lap) guarda órdenes en un array y las cuelga de un nivel de precio. Dentro del nivel el enlace es FIFO: la primera de la cola es la más vieja. Cada lado tiene sus niveles.

`Order` es tres `i32`: 12 bytes. `Level` es tres `i32` y un `u8`: 16 bytes, con el `u8` en el offset 12 y el relleno hasta la alineación de 4. La salida empieza por `size 12 16`.

Los identificadores y los precios llegan como `i64`. El campo es `i32`, así que el store es `id as i32`. `as` ata más que `+`, y por eso el enlace inicial es `(i + 1) as i32`. Un entero más ancho no se estrecha solo.

`cancel 99` no está en el libro e imprime `miss 99`. Cancelar la primera orden de un precio deja la siguiente en la cabeza. El mejor bid es el precio más alto; el mejor ask, el más bajo. Vacío imprime 0. Los precios del ejemplo son positivos.

El banco usa 256 órdenes, 32 niveles y 400000 pasos de un generador lineal. Los tres lenguajes calculan los argumentos antes de la llamada: el orden de evaluación de C no está fijado. Los rangos están en [../RESULTS.md](../RESULTS.md).
