# Propuesta: dos niveles en la misma sintaxis

**Esto es una propuesta. No está implementado.** El compilador de este árbol, cuando se escribió la propuesta, era Copihue 0.19.0 (`compiler/bootstrap-c/copihue.c`).

**Corte 0.41.0.** Un trait (`trait Ord`, `impl Ord for i64`, `fn max<T: Ord>`) se monomorfiza: la copia llama al símbolo del impl. `x.f()` es `f(x)`. `enum Option<T>` existe si el campo es un entero o un puntero. El detalle está en [`DESIGN_TRAITS.md`](DESIGN_TRAITS.md).

**Corte 0.40.0.** Una función o un struct puede ser genérico (`fn max<T>`, `struct Par<A, B>`). El compilador emite una copia por combinación usada. El detalle está en [`DESIGN_GENERICS.md`](DESIGN_GENERICS.md).

**Corte 0.39.0.** Un enum se devuelve por un puntero oculto en `rdi` (no por `rax:rdx`: `Pair` mide 24) y se pasa por valor. `match` acepta `_`, un bloque, un global y un campo (`s.o` y `p->o`). En hosted `-O2` la etiqueta se carga una vez en `rbx`. El detalle está en [`DESIGN_ENUMS.md`](DESIGN_ENUMS.md).

**Corte 0.38.0.** `enum` con campos enteros o punteros, y `match` que exige todas las variantes. El valor es etiqueta más payload en un local o un parámetro: sin heap y sin libc, también en `--freestanding` y `--flat`. `Option` lo escribe el programa; no es un tipo del compilador. No se devuelve un enum, no vive en un global, ni en un campo, ni en un slice, un array o un `vec`. El detalle está en [`DESIGN_ENUMS.md`](DESIGN_ENUMS.md).

**Corte 0.37.0.** Sobre el corte anterior: `vec T` acepta un struct, `vec_push` copia el valor desde una dirección (un local, un índice o un campo; un literal no tiene dirección), `let s: []T = v` copia el puntero y el largo sin quedarse el buffer, y `own *T` se puede devolver y pasar como parámetro. Ese paso mueve: el dueño anterior queda en nulo y no libera dos veces. Un parámetro sin tipo sigue siendo `i64`. Indexarlo o desreferenciarlo no infiere `*T`: un entero más chico sigue siendo válido en un destino más ancho, `*i8` y `*u8` son el mismo ancho, y `*p` de un puntero no volátil es una carga de 64 bits. No hay un solo tipo que salga de ese uso.

**Corte 0.36.0.** Esta nota sigue siendo la propuesta de 0.19, con un corte encima. Lo que el compilador tomó: un parámetro sin `: T` y una función sin `->` son `i64`, y la anotación escrita sigue mandando (`let` ya podía omitir el tipo de un entero o de un puntero). `let v: vec T` es un local hosted de 24 bytes (puntero, largo, capacidad) que crece con `vec_push` mediante el `malloc`/`free` que ya había, sin libc. `let p: own *T` es un puntero local hosted que se libera al salir de su bloque, y también en `return`, `break` y `continue`. `for x in` sobre un slice ya estaba en 0.34.0; sobre un `vec` copia el puntero y el largo al entrar. Lo que no se metió, porque no sale limpio de lo que ya hay: inferir `*i64` porque el cuerpo indexa el parámetro (`fn sumar(datos, n)` sigue siendo `i64` e indexar `datos` es un error), mover un dueño, liberar al reasignar, devolver o copiar un `own`, un `vec` de structs, y usar `vec` u `own *T` como parámetro, retorno, global o campo. El resto de esta nota no está implementado. Acepta una sola sintaxis. No hay un interruptor de "modo fácil" ni de "modo explícito". Las secciones de abajo describen cómo se verían esos dos niveles encima de la sintaxis actual, qué decidiría el compilador cuando el programador no elige, y en qué orden conviene atacar el tiempo de ejecución según la corrida de [`bench/RESULTS.md`](../bench/RESULTS.md).

El objetivo dicho por Ignacio —un lenguaje que llegue a vencer a C++, Rust y OCaml, con un nivel más simple que Python y otro donde el programador fija tamaños, layout, secciones y `volatile`— no se cumple con el compilador de hoy. La corrida de `bench/` mide C (`gcc -O2`), Rust y OCaml, no C++. En tiempo de ejecución Copihue pierde las ocho filas.

## Los dos niveles son el mismo programa

El modo fácil es el programa sin anotaciones de representación. El modo explícito es ese programa con las anotaciones escritas. Si la anotación está, manda el programador. Si falta, manda el defecto de la tabla de más abajo.

### Fibonacci

Modo fácil. **No compila hoy**: una función exige el tipo de cada parámetro y el tipo de retorno.

```copihue
fn fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

Modo explícito. Esto es Copihue 0.19 y es `bench/src/fib.lap`.

```copihue
fn fib(n: i64) -> i64 {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

El defecto propuesto para un entero sin anotación es `i64`, que es lo que el compilador ya hace con `let x = 5`. Escribir `: i32` es la pisada. Hoy esa pisada no cambia la cuenta: la referencia dice que toda la aritmética ocurre en registros de 64 bits y que el tipo angosto solo afecta la carga y el almacén.

### El mismo recorrido de memoria

Modo fácil, propuesto. **No compila hoy** (faltan los tipos de `sumar`).

```copihue
fn sumar(datos, n) {
    let total = 0;
    let i = 0;
    while (i < n) {
        total = total + datos[i];
        i = i + 1;
    }
    return total;
}
```

Defecto propuesto: `total`, `i` y `n` son `i64`; `datos` es `*i64`; el acceso no es volátil; el arreglo o el buffer vive en RAM normal. Cuando exista asignación de registros, una carga no volátil puede quedar en un registro.

Modo explícito. El tipo `*u32`, el índice y el bucle ya se aceptan. La suma del valor cargado sigue siendo de 64 bits.

```copihue
fn sumar(datos: *u32, n: i64) -> i64 {
    let total: i64 = 0;
    let i: i64 = 0;
    while (i < n) {
        total = total + datos[i];
        i = i + 1;
    }
    return total;
}
```

El mismo bucle contra un puerto solo tiene sentido en explícito. `volatile` **no existe** en 0.19. Hace falta antes de cachear cargas: si el compilador guarda `lsr[0]` en un registro, el `while` de un registro de estado deja de ver el hardware.

```copihue
fn serial_putc(c: i64) -> i64 {
    let lsr: *volatile u8 = 0x3FD;
    let thr: *volatile u8 = 0x3F8;
    while ((lsr[0] & 0x20) == 0) { }
    thr[0] = c;
    return 0;
}
```

El programa de puerto que sí compila hoy está en el README y usa `inb` / `outb`, no un puntero volátil.

Una dirección de carga y una sección tampoco son modo fácil. Hoy existen como bandera, `copihue --flat ADDR`, no como sintaxis. La forma propuesta, que **no compila**:

```copihue
section ".boot" @ 0x7C00 {
    fn _start() -> i64 {
        return 0;
    }
}
```

Un struct con layout explícito, también propuesto y **sin implementar**. Hoy cada campo escalar ocupa 8 bytes, en orden, y `a = b` entre structs no copia los campos (la referencia lo lista como no soportado).

```copihue
struct IdtEntry {
    offset_lo: u16 @ 0,
    selector: u16 @ 2,
    flags: u16 @ 4,
    offset_hi: u16 @ 6
}
```

## Defectos y pisadas

| Decisión | Defecto del modo fácil | Cómo lo pisa el programador | Copihue 0.19.0 |
|---|---|---|---|
| Ancho del entero y de la cuenta | `i64`, aritmética a 64 bits | `: i8`, `: i16`, `: i32`, `: u8`, `: u16`, `: u32`, `: u64` | la anotación se acepta; la cuenta sigue en 64 bits |
| Arreglo | elemento `i64`, local, en la pila | `[u8; N]`, global, `*T` | el ancho del elemento sí cambia carga y almacén |
| Volátil | no | `*volatile T` | no existe la palabra |
| Struct | campos en orden, 8 bytes por escalar | ancho del campo y `@ offset`; asignación `a = b` | layout de 8 bytes; la copia `a = b` no está hecha |
| Dónde vive el código | ELF hosted, entra por `main` | `section` y dirección; hoy, `--freestanding` y `--flat` | solo por la línea de comandos |
| Registros | los elige el compilador | no se nombran registros en el fuente; `asm("...")` ya inserta texto NASM | no los elige: cada operación hace `push` / `pop` y cada local se recarga |

El modo fácil no debe estrechar un entero por su cuenta. El desborde cambiaría. Hasta el chequeo de tipos del punto 2 de la hoja, el defecto es el de hoy: 64 bits. Un ancho menor es una anotación, no una inferencia silenciosa.

`let` ya puede omitir el tipo. Las funciones no. El modo fácil, en esta propuesta, es extender esa omisión a parámetros y al tipo de retorno, sin inventar otra gramática.

## Hoja de ruta ya acordada

El orden de abajo es el que ya está acordado. Los tiempos de `bench/` no lo reordenan: esa hoja es de correctitud y de sistemas. El apartado siguiente es solo el orden en que el tiempo de esta tabla puede bajar.

1. **Arrays locales en freestanding, asignación de structs y `volatile` en punteros.** El modo explícito los necesita para un kernel. Un array local hosted ya funciona: `array`, `sort`, `matmul` y `sieve` son eso. En freestanding, el camino de error del índice de un array local emite `syscall` (el generador de `copihue.c` escribe esa llamada en el fallo del chequeo). Un array local con ese camino no es freestanding. Además, un local `[u8; N]` reserva `N * 8` bytes de marco: el hueco se redondea al número de elementos y cada hueco mide 8. La criba de 400000 elementos cupo en el `ulimit -s` de 8192 KB de la corrida; el dato está en `bench/RESULTS.md`. `volatile` no cambia el código de hoy, porque ninguna carga se queda en un registro. Tiene que existir antes de que el paso de rendimiento 1 empiece a cachearlas.

2. **Chequeo de tipos real y aritmética con el ancho escrito.** Sin esto, `: i32` no es el modo explícito: es un comentario que el generador no obedece en la cuenta. Este es el cimiento de los dos niveles. La suite no lo mide. Todos los valores caben en 64 bits y en el `int` de 63 bits de OCaml en amd64, y las cuatro salidas de cada programa coincidieron.

3. **Secciones y direcciones de carga con nombre.** Modo explícito. `--flat ADDR` es el anticipo por CLI. Los ocho programas son hosted; esta fila no sale de la tabla.

4. **Módulos y ELF reubicable.** Un archivo explícito (layout, sección) se podría usar desde código fácil. No hay un número de `bench/` que este paso mueva.

5. **Arranque en 16 y 32 bits.** Solo explícito: ancho, sección, dirección. Se mide con un binario que arranca donde se le dijo, no con `fib`.

## Qué midió `bench/`

Las cifras de esta sección son la corrida de Copihue 0.19.0 del 2026-10-25 entre 12:50:52 y 12:51:48 UTC. [`bench/RESULTS.md`](../bench/RESULTS.md) lo vuelve a escribir `make bench`, así que una corrida posterior no sustituye los números de abajo. Muestras del 2026-10-25 entre 12:50:52 y 12:51:48 UTC. Linux 6.12.94+ x86_64, CPU que `/proc/cpuinfo` llama Intel Xeon a 2700.000 MHz, 4 CPUs, ejecución con `taskset -c 0`. gcc 13.3.0 `-O2` con `-march=x86-64`, rustc 1.83.0 `-O` (opt-level 2), ocamlopt 4.14.1 `-O3` con `flambda=false`, Copihue 0.19.0 `-O2`. Mediana de 7 ejecuciones tras un calentamiento; entre paréntesis, en el archivo de resultados, el mínimo y el máximo. Las cuatro versiones de cada programa imprimieron la misma línea.

Ejecución, mediana en milisegundos:

| Programa | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|
| `chain` | 206.2 | 189.4 | 183.7 | 189.2 | 1.09 | 1.12 | 1.09 |
| `sieve` | 183.2 | 72.3 | 47.1 | 130.6 | 2.53 | 3.89 | 1.40 |
| `matmul` | 174.9 | 35.5 | 53.7 | 82.7 | 4.93 | 3.26 | 2.11 |
| `arith` | 239.8 | 60.4 | 57.4 | 80.5 | 3.97 | 4.18 | 2.98 |
| `array` | 264.5 | 43.3 | 35.0 | 60.0 | 6.10 | 7.56 | 4.41 |
| `sort` | 493.8 | 75.2 | 55.6 | 158.9 | 6.57 | 8.88 | 3.11 |
| `fib` | 745.6 | 141.9 | 199.1 | 370.3 | 5.25 | 3.74 | 2.01 |
| `fold` | 751.2 | 1.8 | 1.7 | 213.0 | 422.75 | 432.02 | 3.53 |

`chain` es una sola cadena `x = (x + i) & máscara; x = (x * 13) & máscara; x = x ^ (x >> 7)`. Los cuatro tiempos quedan juntos. El `push`/`pop` de Copihue casi no se ve cuando la latencia del producto manda.

`fold` es `s = s + i * 3 + 1` durante 500000000 vueltas. El decimal `374999999750000000` está en el ensamblado de `gcc -O2 -S` y de `rustc -O --emit=asm`. No está en el de `ocamlopt -O3 -S`. A 1 ciclo por vuelta y 2700 MHz ese bucle no cabe en 1.8 ms (harían falta unos 185 ms). C y Rust no ejecutaron el bucle. OCaml sí, y frente a OCaml el cociente es 3.53, no 400.

En los bucles que los cuatro sí ejecutan, el hueco grande está en `sort` (8.88× contra Rust, 6.57× contra gcc), `array` (7.56× y 6.10×), `fib` (5.25× contra gcc) y `matmul` (4.93× contra gcc). `arith`, tres acumuladores con `&`, queda entre 3× y 4×.

Compilación de estos archivos, mediana: Copihue entre 7.0 y 9.7 ms en las ocho filas; gcc entre 26.7 y 43.3; ocamlopt entre 63.0 y 73.1; rustc entre 99.5 y 117.9. Es el arranque de un compilador de una pasada más `nasm` y `ld`. No dice nada de un proyecto grande.

Tamaño del fichero: Copihue entre 9616 y 10528 bytes, estático, sin libc. C ronda 16 KB y enlaza libc. Rust ronda 3.8 MB; `file` reporta información de depuración. OCaml ronda 1.1 MB, también con depuración. La columna `text` de `size` va de 431 a 1514 bytes en Copihue, de 1413 a 2743 en C, unos 331 KB en Rust y 621–652 KB en OCaml. Copihue es el más chico en las dos lecturas. El tamaño no es "bytes del algoritmo": el enlace no es el mismo.

`bench/src/array_global.lap` repite `array` con el arreglo en una global. La lectura local emite chequeo de límites (`test` / `cmp` / `jmp`); la global, en este generador, no. Medianas: local 264.5 ms (una de las siete muestras llegó a 377.5), global 263.0 ms. Esta corrida no separa el coste del chequeo del resto.

Al compilar `bench/src/fib.lap` con `copihue -O2 -S`, el marco es `__frame_fib equ 80` y el argumento se guarda en `[rbp-8]` antes de usarlo. Eso no es un tiempo; es el código que produjo esa fuente.

## Pasos medibles, ordenados por esta corrida

1. **Asignación de registros.** El temporal de una operación y el acumulador de un bucle se quedan en registros, en vez de `push` / `pop` y de recargar el local en cada uso. Es el paso de más alcance sobre las filas cuyo bucle sí corre: `arith`, `array`, `sort`, `matmul`, `fib`, y el 3.53× de `fold` contra OCaml. `chain` debe seguir cerca de 1×, porque ya está limitado por la multiplicación; si `chain` se dispara, la asignación empeoró el caso que ya estaba bien. Se comprueba volviendo a correr `make bench` y leyendo esas medianas. No hace falta una suite nueva.

2. **Llamadas más cortas.** Con el paso 1, `fib` puede dejar el argumento en `rdi` cuando el cuerpo no lo pisa, y el marco puede bajar de esos 80 bytes. Se mide en `fib`. Una llamada recursiva de dos ramas no se convierte en un salto de cola; el margen está en el prólogo y en no volcar `n` a la pila para volver a leerlo.

3. **Variables de inducción y direcciones invariantes.** Es lo que borra `fold` en gcc y en rustc, y lo que simplifica `i * n + k` cuando `n` no cambia. Se mide en `fold`: o el tiempo deja de ser el de 500 millones de vueltas, o el inmediato aparece en el ensamblado de Copihue. Y en `matmul`, donde el índice se recalcula con la máquina de pila en cada acceso. Este paso no sustituye al 1: contra OCaml, que sí ejecuta `fold`, el hueco de hoy es 3.53× y cabe en registros; las 400× contra gcc son este paso 3.

4. **El chequeo de límites no va primero por tiempo.** La mediana de `array` no se movió al quitarlo (global contra local). Sí hay que sacar el `syscall` del camino de error para que un array local sea usable en freestanding. Eso es el punto 1 de la hoja, de correctitud.

5. **`volatile` a la vez que el paso 1.** Cachear una carga sin tener la palabra deja al modo explícito sin forma de nombrar un puerto. El generador puede tratar `*volatile T` como carga y almacén reales aunque, hasta el paso 1, todo acceso ya lo sea.

6. **Chequeo de tipos y aritmética con ancho (punto 2 de la hoja).** No está en la tabla como velocidad. Hace que la anotación explícita sea verdad. Los dos niveles, sin esto, son una convención de estilo.

7. **Secciones con nombre, módulos, ELF reubicable, arranque de 16/32 bits (puntos 3, 4 y 5 de la hoja).** No tienen fila en `bench/`. Se comprueban con un binario freestanding cuya entrada queda en la dirección pedida, no reejecutando `fib`.

Volver a correr `make bench` después del paso 1 es el experimento. Hasta que esas medianas bajen, Copihue no está más cerca de ganarles en ejecución a C, Rust u OCaml: está medido del lado que pierde, salvo en el tiempo de compilar estos archivos chicos y en el tamaño del ejecutable.
