# vec

[vec.lap](../vec.lap) parte de un `vec i64` vacío, empuja `0..99` con `vec_push` y suma el recorrido con `for x in`. El primer crecimiento reserva 4 elementos y los siguientes doblan, así que 100 elementos quedan en capacidad 128. La demo imprime `100 4950 128`: longitud, suma y capacidad.

El banco [../bench/vec.lap](../bench/vec.lap) empuja `0..999999` y suma. No llama a nada parecido a `reserve`. La suma es 499999500000; se imprime partida en `499999 500000` para que cada campo tenga menos de 9 dígitos, y después la longitud `1000000`. C++ es `std::vector` sin `reserve`. Rust es `Vec` sin `with_capacity`. Los tres van con `taskset -c 0`. Los rangos están en [../RESULTS.md](../RESULTS.md).
