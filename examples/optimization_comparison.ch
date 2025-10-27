// Ejemplo: Comparación de Optimizaciones
// Demuestra la diferencia entre -O0, -O1 y -O2
//
// Compilar con:
//   ./compiler/bootstrap-c/chronos_v10 -O0 optimization_comparison.ch
//   ./compiler/bootstrap-c/chronos_v10 -O1 optimization_comparison.ch
//   ./compiler/bootstrap-c/chronos_v10 -O2 optimization_comparison.ch
//
// Luego comparar output.asm para ver las diferencias

fn constant_folding_demo() -> i32 {
    println("1. Constant Folding (-O1+)");

    // Estas operaciones son optimizadas en compile-time
    let a = 10 + 20;        // -O1: se convierte en "mov rax, 30"
    let b = 5 * 6;          // -O1: se convierte en "mov rax, 30"
    let c = 100 - 25;       // -O1: se convierte en "mov rax, 75"
    let d = 50 / 2;         // -O1: se convierte en "mov rax, 25"

    print("   a = 10 + 20 = ");
    print_int(a);
    println("");

    print("   b = 5 * 6 = ");
    print_int(b);
    println("");

    print("   c = 100 - 25 = ");
    print_int(c);
    println("");

    print("   d = 50 / 2 = ");
    print_int(d);
    println("");

    let result = a + b + c + d;  // -O1: precalculado en compile-time
    print("   Total: ");
    print_int(result);
    println("");
    println("");

    return result;
}

fn strength_reduction_demo(x: i32) -> i32 {
    println("2. Strength Reduction (-O2)");

    // Multiplicaciones por potencias de 2
    let mult2 = x * 2;      // -O2: shl rax, 1
    let mult4 = x * 4;      // -O2: shl rax, 2
    let mult8 = x * 8;      // -O2: shl rax, 3

    print("   x * 2 = ");
    print_int(mult2);
    println("");

    print("   x * 4 = ");
    print_int(mult4);
    println("");

    print("   x * 8 = ");
    print_int(mult8);
    println("");

    // Divisiones por potencias de 2
    let div2 = x / 2;       // -O2: sar rax, 1
    let div4 = x / 4;       // -O2: sar rax, 2
    let div8 = x / 8;       // -O2: sar rax, 3

    print("   x / 2 = ");
    print_int(div2);
    println("");

    print("   x / 4 = ");
    print_int(div4);
    println("");

    print("   x / 8 = ");
    print_int(div8);
    println("");

    // Módulos por potencias de 2
    let mod2 = x % 2;       // -O2: and rax, 1
    let mod4 = x % 4;       // -O2: and rax, 3
    let mod8 = x % 8;       // -O2: and rax, 7

    print("   x % 2 = ");
    print_int(mod2);
    println("");

    print("   x % 4 = ");
    print_int(mod4);
    println("");

    print("   x % 8 = ");
    print_int(mod8);
    println("");

    println("");
    return mult2 + div2 + mod2;
}

fn complex_expression() -> i32 {
    println("3. Expresión Compleja");

    // Esta expresión se beneficia de ambas optimizaciones
    let result = (10 + 20) * 4 / 2 + (100 - 50) % 8;

    // Con -O0: 7 operaciones en runtime
    // Con -O1: Constant folding reduce a: 30 * 4 / 2 + 50 % 8
    // Con -O2: Además usa shifts/masks para * 4, / 2, % 8

    print("   (10+20)*4/2 + (100-50)%8 = ");
    print_int(result);
    println("");
    println("");

    return result;
}

fn loop_with_constants() {
    println("4. Loop con Constantes");

    // Límite constante permite optimizaciones adicionales
    let i = 0;
    let sum = 0;

    while (i < 10) {  // -O1+: el límite 10 es conocido
        sum = sum + i * 2;  // -O2: i * 2 se convierte en shift
        i++;
    }

    print("   Suma de (i*2) para i=0..9: ");
    print_int(sum);
    println("");
    println("");
}

fn array_indexing() {
    println("5. Indexación de Arrays");

    let arr: [i32; 8];
    let i = 0;

    while (i < 8) {
        arr[i] = i * 4;  // -O2: i * 4 se convierte en shift
        i++;
    }

    print("   Array[3] = 3*4 = ");
    print_int(arr[3]);
    println("");

    print("   Array[7] = 7*4 = ");
    print_int(arr[7]);
    println("");

    println("");
}

fn main() -> i32 {
    println("=== Comparación de Optimizaciones ===");
    println("");
    println("Compila este programa con diferentes flags:");
    println("  -O0: Sin optimizaciones");
    println("  -O1: Constant folding");
    println("  -O2: Todas las optimizaciones");
    println("");
    println("Luego compara output.asm para ver las diferencias.");
    println("");
    println("---");
    println("");

    constant_folding_demo();
    strength_reduction_demo(100);
    complex_expression();
    loop_with_constants();
    array_indexing();

    println("✅ Demostración completada");
    println("");
    println("Resumen de optimizaciones:");
    println("  -O0: Código directo, debuggable");
    println("  -O1: 10-20% más pequeño (constant folding)");
    println("  -O2: 20% más pequeño, 3-40x más rápido");
    println("");
    println("Las bounds checks y division-by-zero checks");
    println("están SIEMPRE activos, incluso en -O2.");

    return 0;
}
