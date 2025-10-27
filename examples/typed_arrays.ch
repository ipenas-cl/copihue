// Ejemplo: Arrays Locales Tipados
// Demuestra arrays de diferentes tipos (i8, i16, i32, i64)

fn demo_i8_array() {
    println("1. Array de i8 (bytes)");

    let bytes: [i8; 5];
    bytes[0] = 72;   // 'H'
    bytes[1] = 101;  // 'e'
    bytes[2] = 108;  // 'l'
    bytes[3] = 108;  // 'l'
    bytes[4] = 111;  // 'o'

    print("   Valores ASCII: ");
    let i = 0;
    while (i < 5) {
        print_int(bytes[i]);
        if (i < 4) {
            print(", ");
        }
        i++;
    }
    println("");
    println("");
}

fn demo_i16_array() {
    println("2. Array de i16 (short)");

    let numbers: [i16; 4];
    numbers[0] = 100;
    numbers[1] = 200;
    numbers[2] = 300;
    numbers[3] = 400;

    print("   Valores: ");
    let i = 0;
    while (i < 4) {
        print_int(numbers[i]);
        if (i < 3) {
            print(", ");
        }
        i++;
    }
    println("");

    // Suma
    let sum = 0;
    let j = 0;
    while (j < 4) {
        sum += numbers[j];
        j++;
    }
    print("   Suma total: ");
    print_int(sum);
    println("");
    println("");
}

fn demo_i32_array() {
    println("3. Array de i32 (int)");

    let data: [i32; 6];
    let i = 0;
    while (i < 6) {
        data[i] = (i + 1) * 1000;
        i++;
    }

    print("   Valores: ");
    let j = 0;
    while (j < 6) {
        print_int(data[j]);
        if (j < 5) {
            print(", ");
        }
        j++;
    }
    println("");

    // Encontrar máximo
    let max = data[0];
    let k = 1;
    while (k < 6) {
        if (data[k] > max) {
            max = data[k];
        }
        k++;
    }
    print("   Máximo: ");
    print_int(max);
    println("");
    println("");
}

fn demo_i64_array() {
    println("4. Array de i64 (long)");

    let big_numbers: [i64; 3];
    big_numbers[0] = 1000000;
    big_numbers[1] = 2000000;
    big_numbers[2] = 3000000;

    print("   Valores: ");
    let i = 0;
    while (i < 3) {
        print_int(big_numbers[i]);
        if (i < 2) {
            print(", ");
        }
        i++;
    }
    println("");

    // Producto
    let product = big_numbers[0];
    let j = 1;
    while (j < 3) {
        product = product + big_numbers[j];
        j++;
    }
    print("   Suma: ");
    print_int(product);
    println("");
    println("");
}

fn demo_multi_dimensional() {
    println("5. Simulación de Array 2D (3x3)");

    // Array 1D que simula 2D: [3][3] = 9 elementos
    let matrix: [i32; 9];

    // Inicializar matriz 3x3
    matrix[0] = 1; matrix[1] = 2; matrix[2] = 3;
    matrix[3] = 4; matrix[4] = 5; matrix[5] = 6;
    matrix[6] = 7; matrix[7] = 8; matrix[8] = 9;

    // Imprimir matriz
    println("   Matriz:");
    let row = 0;
    while (row < 3) {
        print("   ");
        let col = 0;
        while (col < 3) {
            let index = row * 3 + col;  // Fórmula: row * width + col
            print_int(matrix[index]);
            if (col < 2) {
                print(" ");
            }
            col++;
        }
        println("");
        row++;
    }

    // Suma de diagonal
    let diagonal_sum = matrix[0] + matrix[4] + matrix[8];  // [0][0], [1][1], [2][2]
    print("   Suma diagonal: ");
    print_int(diagonal_sum);
    println("");
    println("");
}

fn main() -> i32 {
    println("=== Arrays Locales Tipados en Chronos ===");
    println("");

    demo_i8_array();
    demo_i16_array();
    demo_i32_array();
    demo_i64_array();
    demo_multi_dimensional();

    println("✅ Todos los tipos de array funcionan correctamente");
    println("");
    println("Nota: Los arrays son de tamaño fijo y bounds-checked.");

    return 0;
}
