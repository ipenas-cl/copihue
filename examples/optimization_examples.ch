// CHRONOS - EJEMPLOS DE OPTIMIZACIÓN
// Comparación: código no optimizado vs optimizado

// ============================================
// EJEMPLO 1: Evitar Divisiones
// ============================================

fn unoptimized_division() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ❌ División en cada iteración (lento: 20-40 ciclos/div)
    while (i < 10000) {
        let half = i / 2;
        sum += half;
        i++;
    }
    
    return sum;
}

fn optimized_division() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ✅ Usar shift (si fuera soportado) o simplificar
    // En este caso: sum = 0 + 0 + 1 + 1 + 2 + 2 + ...
    // Podemos calcular directamente
    while (i < 10000) {
        if (i % 2 == 0) {
            sum += i / 2;
        } else {
            sum += (i - 1) / 2;
        }
        i++;
    }
    
    // Mejor aún: sin división en loop
    let j = 0;
    let result = 0;
    while (j < 5000) {
        result += j;
        result += j;
        j++;
    }
    
    return result;
}

// ============================================
// EJEMPLO 2: Loop Unrolling
// ============================================

fn unoptimized_loop() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ❌ Muchos saltos condicionales
    while (i < 1000) {
        sum += i;
        i++;
    }
    
    return sum;
}

fn optimized_loop() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ✅ Loop unrolling: menos saltos
    while (i < 1000) {
        sum += i;
        sum += i + 1;
        sum += i + 2;
        sum += i + 3;
        i += 4;
    }
    
    return sum;
}

// ============================================
// EJEMPLO 3: Usar Potencias de 2
// ============================================

fn unoptimized_powers() -> i32 {
    let x = 12345;
    
    // ❌ Divisiones/multiplicaciones arbitrarias
    let a = x * 3;
    let b = x / 7;
    let c = x % 13;
    
    return a + b + c;
}

fn optimized_powers() -> i32 {
    let x = 12345;
    
    // ✅ Usar potencias de 2 cuando posible
    let a = x * 4;   // Potencia de 2: puede optimizarse a shift
    let b = x / 8;   // Potencia de 2: puede optimizarse a shift
    let c = x % 16;  // Potencia de 2: puede optimizarse a AND
    
    return a + b + c;
}

// ============================================
// EJEMPLO 4: Cache-Friendly Access
// ============================================

let matrix: [i32; 10000];  // 100x100 matriz

fn unoptimized_matrix() -> i32 {
    // ❌ Acceso por columnas (cache miss)
    let sum = 0;
    let j = 0;
    while (j < 100) {
        let i = 0;
        while (i < 100) {
            sum += matrix[i * 100 + j];  // Stride grande
            i++;
        }
        j++;
    }
    
    return sum;
}

fn optimized_matrix() -> i32 {
    // ✅ Acceso por filas (cache hit)
    let sum = 0;
    let i = 0;
    while (i < 100) {
        let j = 0;
        while (j < 100) {
            sum += matrix[i * 100 + j];  // Acceso secuencial
            j++;
        }
        i++;
    }
    
    return sum;
}

// ============================================
// EJEMPLO 5: Minimizar Bounds Checks
// ============================================

let data: [i32; 1000];

fn unoptimized_checks() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ❌ Bounds check en cada acceso
    while (i < 1000) {
        sum += data[i];     // Check 1
        sum += data[i];     // Check 2 (redundante)
        sum += data[i];     // Check 3 (redundante)
        i++;
    }
    
    return sum;
}

fn optimized_checks() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ✅ Reutilizar valor (solo 1 bounds check)
    while (i < 1000) {
        let val = data[i];  // 1 bounds check
        sum += val;
        sum += val;
        sum += val;
        i++;
    }
    
    return sum;
}

// ============================================
// EJEMPLO 6: Precalcular Constantes
// ============================================

fn unoptimized_constants() -> i32 {
    let sum = 0;
    let i = 0;
    
    // ❌ Recalcula constante en cada iteración
    while (i < 1000) {
        let limit = 100 * 100;  // Calculado 1000 veces
        if (i < limit) {
            sum += i;
        }
        i++;
    }
    
    return sum;
}

fn optimized_constants() -> i32 {
    let sum = 0;
    let i = 0;
    let limit = 100 * 100;  // ✅ Calculado una vez
    
    while (i < limit) {
        sum += i;
        i++;
    }
    
    return sum;
}

// ============================================
// COMPARACIÓN
// ============================================

fn compare_all() -> i32 {
    println("========================================");
    println("  OPTIMIZATION EXAMPLES");
    println("========================================");
    println("");
    
    println("1. Division:");
    print("   Unoptimized: ");
    print_int(unoptimized_division());
    println("");
    print("   Optimized:   ");
    print_int(optimized_division());
    println("");
    println("");
    
    println("2. Loop Unrolling:");
    print("   Unoptimized: ");
    print_int(unoptimized_loop());
    println("");
    print("   Optimized:   ");
    print_int(optimized_loop());
    println("");
    println("");
    
    println("3. Powers of 2:");
    print("   Unoptimized: ");
    print_int(unoptimized_powers());
    println("");
    print("   Optimized:   ");
    print_int(optimized_powers());
    println("");
    println("");
    
    println("4. Cache Access:");
    print("   Unoptimized: ");
    print_int(unoptimized_matrix());
    println("");
    print("   Optimized:   ");
    print_int(optimized_matrix());
    println("");
    println("");
    
    println("5. Bounds Checks:");
    print("   Unoptimized: ");
    print_int(unoptimized_checks());
    println("");
    print("   Optimized:   ");
    print_int(optimized_checks());
    println("");
    println("");
    
    println("6. Constants:");
    print("   Unoptimized: ");
    print_int(unoptimized_constants());
    println("");
    print("   Optimized:   ");
    print_int(optimized_constants());
    println("");
    println("");
    
    println("========================================");
    println("  KEY TAKEAWAYS");
    println("========================================");
    println("1. Avoid divisions in hot loops");
    println("2. Use powers of 2 when possible");
    println("3. Access memory sequentially (cache)");
    println("4. Reuse values to minimize checks");
    println("5. Precalculate constants");
    println("6. Unroll loops for fewer branches");
    
    return 0;
}

fn main() -> i32 {
    return compare_all();
}
