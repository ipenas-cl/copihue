// CHRONOS BENCHMARK SUITE
// Mide performance de diferentes operaciones

let iterations = 1000000;

// ============================================
// BENCHMARK 1: Arithmetic Operations
// ============================================

fn benchmark_addition() -> i32 {
    println("Benchmark 1: Addition (1M iterations)");
    
    let sum = 0;
    let i = 0;
    while (i < iterations) {
        sum += i;
        i++;
    }
    
    print("  Result: ");
    print_int(sum);
    println("");
    return sum;
}

fn benchmark_multiplication() -> i32 {
    println("Benchmark 2: Multiplication (1M iterations)");
    
    let product = 1;
    let i = 1;
    while (i < 100) {  // Menos iteraciones para evitar overflow
        product *= 2;
        product /= 2;  // Mantener número manejable
        i++;
    }
    
    print("  Result: ");
    print_int(product);
    println("");
    return product;
}

fn benchmark_division() -> i32 {
    println("Benchmark 3: Division (100K iterations)");
    
    let result = 1000000;
    let i = 0;
    while (i < 100000) {
        result /= 2;
        result *= 2;  // Mantener número
        i++;
    }
    
    print("  Result: ");
    print_int(result);
    println("");
    return result;
}

fn benchmark_modulo() -> i32 {
    println("Benchmark 4: Modulo (100K iterations)");
    
    let sum = 0;
    let i = 0;
    while (i < 100000) {
        sum += i % 10;
        i++;
    }
    
    print("  Result: ");
    print_int(sum);
    println("");
    return sum;
}

// ============================================
// BENCHMARK 2: Array Operations
// ============================================

let array: [i32; 1000];

fn benchmark_array_access() -> i32 {
    println("Benchmark 5: Array Access (100K iterations)");
    
    // Initialize array
    let i = 0;
    while (i < 1000) {
        array[i] = i;
        i++;
    }
    
    // Access array
    let sum = 0;
    let j = 0;
    while (j < 100000) {
        let idx = j % 1000;
        sum += array[idx];
        j++;
    }
    
    print("  Result: ");
    print_int(sum);
    println("");
    return sum;
}

// ============================================
// BENCHMARK 3: Power of 2 Operations
// ============================================

fn benchmark_power_of_2() -> i32 {
    println("Benchmark 6: Power of 2 Operations");
    
    let x = 100000;
    
    // Test multiplication by powers of 2
    let t1 = x * 2;
    let t2 = x * 4;
    let t3 = x * 8;
    let t4 = x * 16;
    
    // Test division by powers of 2
    let t5 = x / 2;
    let t6 = x / 4;
    let t7 = x / 8;
    let t8 = x / 16;
    
    // Test modulo by powers of 2
    let t9 = x % 2;
    let t10 = x % 4;
    let t11 = x % 8;
    let t12 = x % 16;
    
    let sum = t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8 + t9 + t10 + t11 + t12;
    
    print("  Result: ");
    print_int(sum);
    println("");
    return sum;
}

// ============================================
// BENCHMARK 4: Loop Patterns
// ============================================

fn benchmark_nested_loops() -> i32 {
    println("Benchmark 7: Nested Loops");
    
    let sum = 0;
    let i = 0;
    while (i < 100) {
        let j = 0;
        while (j < 100) {
            sum += i + j;
            j++;
        }
        i++;
    }
    
    print("  Result: ");
    print_int(sum);
    println("");
    return sum;
}

// ============================================
// MAIN
// ============================================

fn main() -> i32 {
    println("");
    println("========================================");
    println("  CHRONOS BENCHMARK SUITE");
    println("  Testing: CPU Performance");
    println("========================================");
    println("");
    
    // Run benchmarks
    benchmark_addition();
    println("");
    
    benchmark_multiplication();
    println("");
    
    benchmark_division();
    println("");
    
    benchmark_modulo();
    println("");
    
    benchmark_array_access();
    println("");
    
    benchmark_power_of_2();
    println("");
    
    benchmark_nested_loops();
    println("");
    
    println("========================================");
    println("  BENCHMARKS COMPLETE");
    println("========================================");
    
    return 0;
}
