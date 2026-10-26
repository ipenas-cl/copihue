# Watchdog con latido

[watch.lap](../watch.lap) arranca una hebra que escribe `hb` de 1 a 1000 con `atomic_store_release` y después publica `phase = 1`. Ahí deja de latir y espera el acuse. Main espera ese `phase`, lee el latido y exige 100 muestras iguales. Si alguna difiere, la cuenta de fallos sube y la ventana vuelve a empezar. Con el productor ya detenido la ventana cierra en 100 y la salida es `beat 1000`, `miss 0`.

La ventana es una cuenta de lecturas, no un reloj. Un timeout de pared dependería del planificador y la prueba de oro no sería estable. El productor solo se detiene después del último latido, y el release de `phase` publica ese alto: la carga acquire que lo ve también ve `hb == 1000`.

[../bench/watch.lap](../bench/watch.lap) es el mismo protocolo con 4000000 latidos y 1000000 lecturas quietas. Imprime `4000000 0`. C++ y Rust usan el mismo alto y la misma ventana. El tiempo está en [../RESULTS.md](../RESULTS.md).
