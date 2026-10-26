# Anillo de una hebra

[ring.lap](../ring.lap) es la forma de una cola SPSC: buffer de 8, índices `head` y `tail`, máscara `7` en lugar de un resto. Ocho es potencia de 2. Una ranura se deja vacía, así que caben 7 valores. Lleno y vacío no se confunden: vacío es `head == tail`, lleno es `(head + 1) & 7 == tail`.

`head` y `tail` son `let volatile` de `u32`. Cada lectura y cada escritura del fuente es un acceso a memoria; `-O2` no las saca del bucle ni las pliega. No es un modelo de memoria de C y no hay una barrera más fuerte que la instrucción. Con una sola hebra el resultado es el mismo que con índices normales. La segunda hebra, y el atómico que haría falta para publicarla, no están.

El índice del array es `i64`, así que el `u32` entra con `as`: `slots[h as i64]`. `ring_pull` escribe el valor por un `*i64`, para que un 0 guardado no se confunda con el fallo. La octava inserción imprime `full 0`. Vaciar del todo imprime `empty 0`.

No hay banco. Medir este anillo contra C no diría nada de una cola entre dos hebras.
