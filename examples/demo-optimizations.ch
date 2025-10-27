// DEMO DE OPTIMIZACIONES CHRONOS v0.17
// Compila con -O0, -O1, -O2 y compara los resultados

fn test_constant_folding() -> i32 {
    println("=== Test 1: Constant Folding ===");

    // Estas expresiones se evalúan en compile-time con -O1 y -O2
    let a = 10 + 20;        // 30
    let b = 100 - 25;       // 75
    let c = 5 * 8;          // 40
    let d = 100 / 4;        // 25
    let e = 17 % 5;         // 2

    let resultado = a + b + c + d + e;

    print("  a = 10 + 20 = ");
    print_int(a);
    println("");

    print("  b = 100 - 25 = ");
    print_int(b);
    println("");

    print("  c = 5 * 8 = ");
    print_int(c);
    println("");

    print("  d = 100 / 4 = ");
    print_int(d);
    println("");

    print("  e = 17 % 5 = ");
    print_int(e);
    println("");

    print("  Total: ");
    print_int(resultado);
    println("");

    return resultado;
}

fn test_strength_reduction() -> i32 {
    println("");
    println("=== Test 2: Strength Reduction ===");

    let x = 1000;

    // Estas operaciones se optimizan con -O2
    let a = x * 2;      // Multiplicación → shift izquierda
    let b = x * 4;      // Multiplicación → shift izquierda
    let c = x / 8;      // División → shift derecha
    let d = x % 16;     // Módulo → máscara AND

    print("  x = ");
    print_int(x);
    println("");

    print("  x * 2 = ");
    print_int(a);
    println("");

    print("  x * 4 = ");
    print_int(b);
    println("");

    print("  x / 8 = ");
    print_int(c);
    println("");

    print("  x % 16 = ");
    print_int(d);
    println("");

    let resultado = a + b + c + d;

    print("  Total: ");
    print_int(resultado);
    println("");

    return resultado;
}

fn test_array_processing() -> i32 {
    println("");
    println("=== Test 3: Procesamiento de Arrays ===");

    let datos: [i32; 256];  // Tamaño potencia de 2

    // Inicializar
    let i = 0;
    while (i < 256) {
        datos[i] = i;
        i++;
    }

    // Procesar con módulo (optimizado con -O2)
    let suma = 0;
    let j = 0;
    while (j < 500) {
        let idx = j % 256;  // Módulo por potencia de 2 → AND
        suma += datos[idx];
        j++;
    }

    print("  Elementos procesados: ");
    print_int(j);
    println("");

    print("  Suma total: ");
    print_int(suma);
    println("");

    return suma;
}

fn test_mixed_operations() -> i32 {
    println("");
    println("=== Test 4: Operaciones Mixtas ===");

    // Constantes (folded con -O1)
    let size = 100 * 100;
    let limite = 1024;

    print("  size = 100 * 100 = ");
    print_int(size);
    println("");

    print("  limite = ");
    print_int(limite);
    println("");

    // Operaciones con potencias de 2 (optimized con -O2)
    let mitad = size / 2;
    let cuarto = size / 4;
    let resto = size % 256;

    print("  size / 2 = ");
    print_int(mitad);
    println("");

    print("  size / 4 = ");
    print_int(cuarto);
    println("");

    print("  size % 256 = ");
    print_int(resto);
    println("");

    let resultado = mitad + cuarto + resto;

    print("  Total: ");
    print_int(resultado);
    println("");

    return resultado;
}

fn main() -> i32 {
    println("========================================");
    println("  CHRONOS v0.17 - DEMO OPTIMIZACIONES");
    println("========================================");
    println("");

    let r1 = test_constant_folding();
    let r2 = test_strength_reduction();
    let r3 = test_array_processing();
    let r4 = test_mixed_operations();

    println("");
    println("========================================");
    println("  RESULTADOS FINALES");
    println("========================================");

    print("Test 1 (Constant Folding): ");
    print_int(r1);
    println("");

    print("Test 2 (Strength Reduction): ");
    print_int(r2);
    println("");

    print("Test 3 (Array Processing): ");
    print_int(r3);
    println("");

    print("Test 4 (Mixed Operations): ");
    print_int(r4);
    println("");

    let total = r1 + r2 + r3 + r4;

    println("");
    print("TOTAL: ");
    print_int(total);
    println("");

    println("");
    println("========================================");
    println("  COMPILAR Y COMPARAR");
    println("========================================");
    println("./chronos_v10 -O0 demo_optimizaciones.ch");
    println("./chronos_v10 -O1 demo_optimizaciones.ch");
    println("./chronos_v10 -O2 demo_optimizaciones.ch");
    println("");
    println("Los 3 niveles producen el mismo output!");
    println("========================================");

    return 0;
}
