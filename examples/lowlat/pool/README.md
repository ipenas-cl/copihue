# Pool de bloques fijos

[pool.lap](../pool.lap) es una lista libre de punteros sobre un array de 4 `Block`. `alloc` saca la cabeza y `free` la empuja. Las dos son O(1). No hay `malloc`.

`Block` no es un registro de 8 bytes por campo. `next` es un puntero (8), `tag` es `u8` (1) y `val` es `i64`, alineado a 8. `sizeof(Block)` imprime 24: `val` queda en el offset 16. El índice de una ranura es `(p as i64 - &slots as i64) / sizeof(Block)`. `as` es el paso de puntero a entero; sin él la resta no compila.

La quinta reserva, con las cuatro vivas, imprime `full 0`: el puntero nulo es el literal `0`. Liberar una ranura y reservar otra devuelve esa misma ranura. La que no se liberó conserva `val`.

El banco equivalente, con 1024 bloques y 4000 vueltas, está en [../bench/](../bench/). C y Rust usan el mismo layout (`sizeof` 24). Rust necesita `unsafe` para seguir los punteros. Los rangos están en [../RESULTS.md](../RESULTS.md).
