fn main() -> i32 {
    println("=== Testing && operator ===");

    // Test 1: true && true
    if (1 && 1) {
        println("1. PASS: 1 && 1 = true");
    }

    // Test 2: false && true (should short-circuit)
    if (0 && 1) {
        println("2. FAIL: Should not print");
    } else {
        println("2. PASS: 0 && 1 = false");
    }

    // Test 3: true && false
    if (1 && 0) {
        println("3. FAIL: Should not print");
    } else {
        println("3. PASS: 1 && 0 = false");
    }

    println("");
    println("=== Testing || operator ===");

    // Test 4: true || true
    if (1 || 1) {
        println("4. PASS: 1 || 1 = true");
    }

    // Test 5: true || false (should short-circuit)
    if (1 || 0) {
        println("5. PASS: 1 || 0 = true");
    }

    // Test 6: false || true
    if (0 || 1) {
        println("6. PASS: 0 || 1 = true");
    }

    // Test 7: false || false
    if (0 || 0) {
        println("7. FAIL: Should not print");
    } else {
        println("7. PASS: 0 || 0 = false");
    }

    println("");
    println("=== Testing with comparisons ===");

    let x = 10;
    let y = 20;

    // Test 8: Complex AND
    if (x > 5 && y > 15 && x < 15) {
        println("8. PASS: x>5 && y>15 && x<15");
    }

    // Test 9: Complex OR
    if (x > 100 || y > 15 || x < 0) {
        println("9. PASS: x>100 || y>15 || x<0");
    }

    // Test 10: Mixed AND/OR
    if ((x > 5 && y > 15) || x < 0) {
        println("10. PASS: (x>5 && y>15) || x<0");
    }

    println("");
    println("All tests passed!");
    return 0;
}
