# False sharing

[share.lap](../share.lap) lanza dos hebras. Cada una hace `fetch_add` de su propio `u64`. No hay otro dato compartido.

`Near` es `align(64)` y los campos quedan en su offset natural: `a` en 0, `b` en 8, `sizeof` 64. Los dos viven en la misma línea. `Far` repite `align(64)` en cada campo, así que `b` queda en 64 y `sizeof` es 128. La salida de la demo, con 1000 vueltas por hebra, es `share 64`, `2000`, `split 128`, `2000`.

El tiempo está en [../bench/share.lap](../bench/share.lap) y [../bench/split.lap](../bench/split.lap). Los dos hacen 4000000 `fetch_add` por hebra e imprimen 8000000. C++ y Rust usan `std::atomic` / `AtomicU64` con `Relaxed`, que en x86 es el mismo `lock xadd`. `share` no se mide con `taskset -c 0`: las dos hebras en un solo núcleo no se pelean la línea entre núcleos. Los rangos están en [../RESULTS.md](../RESULTS.md).
