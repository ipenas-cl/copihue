# Afinidad de CPU

[pin.lap](../pin.lap) llama a `sched_setaffinity` en la hebra actual. Cpu 0 devuelve 0. Cpu 1024 no cabe en la máscara de 1024 bits y devuelve -1 sin entrar al kernel. Después dos hebras se fijan a cpu 0 y cpu 1. Cada una incrementa su contador. Los contadores ya van en líneas distintas (`align(64)` en cada campo), para no mezclar este caso con el false sharing. Con 1000 vueltas la suma es 2000.

El banco repite el bucle 4000000 veces e imprime 8000000 solo si las dos llamadas devolvieron 0. Si la afinidad falla, imprime 0.

| Programa | Qué fija |
|---|---|
| [../bench/pin.lap](../bench/pin.lap) | cpu 0 y cpu 1 |
| [../bench/same.lap](../bench/same.lap) | las dos hebras en cpu 0 |
| [../bench/free.lap](../bench/free.lap) | nada: el planificador elige |

C++ usa `sched_setaffinity`. Rust no tiene esa llamada en `std`; el `.rs` hace la syscall 203 con una máscara de 128 bytes, la misma que emite el compilador. No van bajo `taskset -c 0`, porque esa máscara le quita el resto de las CPU al proceso y fijar la hebra a cpu 1 fallaría. Si la máquina tiene una sola CPU, o el contenedor rechaza la máscara, la salida 0 y los rangos no sirven para afirmar el efecto. Los números medidos están en [../RESULTS.md](../RESULTS.md).
