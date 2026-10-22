# Inicio Rápido

**Tiempo**: 5 minutos

---

## 1. Hola Mundo

```bash
# Crear programa
echo 'fn main() -> i32 {
    println("¡Hola, Chronos!");
    return 0;
}' > hola.ch

# Compilar
./compiler/bootstrap-c/chronos_v10 -O2 hola.ch

# Ejecutar
./chronos_program
```

---

## 2. Ejemplo con Arrays

```bash
cat > arrays.ch << 'EOF'
fn main() -> i32 {
    let arr: [i32; 10];

    // Inicializar
    let i = 0;
    while (i < 10) {
        arr[i] = i * 2;
        i++;
    }

    // Sumar
    let suma = 0;
    let j = 0;
    while (j < 10) {
        suma += arr[j];
        j++;
    }

    print("Suma: ");
    print_int(suma);
    println("");

    return 0;
}
EOF

./compiler/bootstrap-c/chronos_v10 -O2 arrays.ch
./chronos_program
```

---

## 3. Demo de Optimizaciones

```bash
./compiler/bootstrap-c/chronos_v10 -O2 demo_optimizaciones.ch
./chronos_program
```

---

## 4. Benchmarks

```bash
./compiler/bootstrap-c/chronos_v10 -O2 examples/benchmarks.ch
time ./chronos_program
```

---

## Flags de Optimización

```bash
-O0  # Sin optimizaciones (debug)
-O1  # Optimizaciones básicas (desarrollo)
-O2  # Todas las optimizaciones (producción)
```

---

## Próximos Pasos

- Lee la [documentación completa](README.md)
- Ver [sintaxis del lenguaje](docs/syntax.md)
- Ver [optimizaciones](docs/optimizations.md)
