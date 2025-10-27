# Chronos

**Lenguaje de programación de sistemas** - Rápido, seguro y determinista.

**Versión**: v0.17

---

## Características

- ✅ Tipos básicos: `i8`, `i16`, `i32`, `i64`
- ✅ Arrays tipados: `[i32; 100]`
- ✅ Funciones, control de flujo (`if`, `while`, `for`)
- ✅ Operadores: `+`, `-`, `*`, `/`, `%`, `&&`, `||`, `!`, `++`, `--`, `+=`
- ✅ **Compilador optimizador** (constant folding, strength reduction)
- ✅ **Bounds checking** siempre activo
- ✅ **100% determinista**

---

## Inicio Rápido

```bash
# 1. Compilar programa
./compiler/bootstrap-c/chronos_v10 -O2 programa.ch

# 2. Ejecutar
./chronos_program
```

### Hola Mundo

```chronos
fn main() -> i32 {
    println("¡Hola, Chronos!");
    return 0;
}
```

---

## Optimizaciones

```bash
-O0  # Sin optimizaciones (debug)
-O1  # Constant folding (desarrollo)
-O2  # Todas las optimizaciones (producción)
```

**Resultados**:
- 20% menos código generado
- 3-40x más rápido en operaciones optimizadas
- 100% correctitud verificada

---

## Documentación

- **[Inicio Rápido](QUICKSTART.md)** - Empezar en 5 minutos
- **[Sintaxis](docs/syntax.md)** - Sintaxis completa
- **[Optimizaciones](docs/optimizations.md)** - Guía de optimizaciones
- **[Características](docs/features.md)** - Lista completa
- **[Standard Library](docs/stdlib.md)** - Funciones disponibles

---

## Ejemplos

### Básicos
- `examples/hello.ch` - Hola mundo
- `examples/arrays.ch` - Trabajo con arrays
- `examples/fibonacci.ch` - Fibonacci
- `examples/fizzbuzz.ch` - FizzBuzz

### Características del Lenguaje
- `examples/logical_operators.ch` - Operadores &&, ||, !
- `examples/typed_arrays.ch` - Arrays de i8, i16, i32, i64
- `examples/file_io_complete.ch` - File I/O completo

### Optimizaciones
- `examples/demo-optimizations.ch` - Demo interactivo
- `examples/optimization_comparison.ch` - Comparación -O0/-O1/-O2
- `examples/benchmark_suite.ch` - Suite de benchmarks

### Avanzados
- `examples/binary_search.ch` - Búsqueda binaria
- `examples/bubble_sort.ch` - Ordenamiento
- `examples/primes.ch` - Números primos
- `examples/gcd.ch` - Algoritmo de Euclides

```bash
# Compilar cualquier ejemplo
./compiler/bootstrap-c/chronos_v10 -O2 examples/logical_operators.ch
./chronos_program
```

---

## Filosofía

1. **Seguridad primero** - Bounds checking, sin undefined behavior
2. **Determinismo** - Mismo input → mismo output, siempre
3. **Simplicidad** - Sintaxis clara, comportamiento predecible
4. **Performance** - Optimizaciones sin comprometer seguridad

---

## Licencia

Ver archivo LICENSE

---

**Chronos v0.17 - Fast, Safe, Deterministic** 🔥
