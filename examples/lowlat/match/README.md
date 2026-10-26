# Motor de emparejamiento

[match.lap](../match.lap) usa el mismo libro. Una orden límite cruza el lado contrario mientras el precio alcance, y el resto queda en el libro. Una orden de mercado cruza cualquier precio y no descansa: lo que no se llena se imprime como `mkt`.

La prioridad es precio y, dentro del precio, tiempo. La cabeza del nivel es la orden más vieja. En la salida, la compra 4 se lleva las 5 de la orden 1 a 100 y solo 1 de la orden 3, que llegó después al mismo precio. Esa orden 3 sigue con 2, y la compra 5 se las lleva a 100 antes de subir a 101. Es el cruce parcial.

La venta de mercado 6 pega al bid 101, no al 90: el mejor precio va primero. La compra de mercado 7 no encuentra asks e imprime `mkt 7 5`. `miss 1` es cancelar un id que no está.

El banco no imprime cada cruce. Suma las cantidades ejecutadas y la máscara final es la de [../RESULTS.md](../RESULTS.md). No hay subasta, ni prevención de auto-cruce, ni hilos.
