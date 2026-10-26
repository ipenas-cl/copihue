# Cola SPSC de dos hebras

[ring.lap](../ring.lap) es la cola de [bench/lowlat/spsc.lap](../../../bench/lowlat/spsc.lap) a escala de prueba. Capacidad 8, potencia de 2. Los índices `write_idx` y `read_idx` son `u64` libres: no se enmascaran al guardarlos. La máscara `7` solo elige la ranura, así que las 8 caben. Lleno es `(write - read) == 8`. Vacío es `write == read`.

Cada índice es `align(64) let volatile`, así que no comparten línea de caché. Publicarlos es `atomic_store_release`. Leerlos es `atomic_load_acquire`. En x86 eso es un `mov`: la carga ya es acquire y el store ya es release. El valor entra en la ranura en una sentencia anterior al release, el mismo orden que el banco. `cpu_pause` espera cuando está llena o vacía. `as i64` convierte el `u64` que devuelve la carga; un cast a 64 bits no emite instrucción. El índice del array es ese `i64`.

Una pasada de una hebra, antes de crear la otra, fija lleno y vacío: la cola vacía devuelve 0, la novena inserción devuelve 0, y vaciar lo que queda suma 44 (2+3+4+5+6+7+8+9). Después `thread_spawn` arranca el productor sobre una pila de 65536 bytes, sin libc. Esa hebra manda 1..24. Main consume, cuenta un valor fuera de orden y suma. La salida es `bad 0` y `sum 300`. `thread_join` espera el futex del `tid`.

El tiempo de una cola de un millón de mensajes no se mide aquí. Está en [bench/lowlat/RESULTS.md](../../../bench/lowlat/RESULTS.md) (0.32.0, ventana 2026-10-26T00:18:42Z): Copihue 45.0 ms (18.4–76.1), C++ 34.1 (11.2–44.4), Rust 15.8 (10.5–30.6). Esos tres rangos se solapan. No hay una segunda copia de ese banco.
