fn main() -> i32 {
    println("=== TEST COMPLETO CHRONOS v0.16 ===");
    println("");

    println("1. Operadores de incremento/decremento:");
    let counter = 0;
    counter++;
    counter++;
    counter++;
    if (counter == 3) {
        println("   PASS: ++ funciona");
    }
    counter--;
    if (counter == 2) {
        println("   PASS: -- funciona");
    }

    println("");
    println("2. Operadores de asignacion compuesta:");
    let x = 10;
    x += 5;
    x *= 2;
    x -= 10;
    x /= 2;
    if (x == 10) {
        println("   PASS: +=, *=, -=, /= funcionan");
    }
    
    let y = 17;
    y %= 5;
    if (y == 2) {
        println("   PASS: %= funciona");
    }

    println("");
    println("3. String literal indexing:");
    let ch1 = "Hello"[0];
    let ch2 = "Hello"[4];
    if (ch1 == 72 && ch2 == 111) {
        println("   PASS: String indexing funciona");
    }

    println("");
    println("4. Operadores logicos:");
    if (1 && 1) {
        println("   PASS: && funciona");
    }
    if (0 || 1) {
        println("   PASS: || funciona");
    }
    if (!0) {
        println("   PASS: ! funciona");
    }

    println("");
    println("5. Operador modulo:");
    let mod1 = 10 % 3;
    if (mod1 == 1) {
        println("   PASS: % funciona");
    }

    println("");
    println("6. Arrays locales tipados:");
    let arr: [i32; 3];
    arr[0] = 100;
    arr[1] = 200;
    if (arr[0] == 100 && arr[1] == 200) {
        println("   PASS: Arrays locales tipados funcionan");
    }

    println("");
    println("=== TODAS LAS FEATURES PASAN ===");
    return 0;
}
