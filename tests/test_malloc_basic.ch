// Test: Basic malloc functionality
// Allocate memory and write/read values

fn main() -> i32 {
    println("=== Test: Basic malloc ===");

    // 1. Allocate 40 bytes (10 i32s)
    let ptr = malloc(40);

    // Check if allocation succeeded
    if (ptr == 0) {
        println("ERROR: malloc failed");
        return 1;
    }

    println("✓ malloc(40) succeeded");
    print("  ptr = 0x");
    print_int(ptr);
    println("");

    // 2. Write values to allocated memory
    ptr[0] = 10;
    ptr[1] = 20;
    ptr[2] = 30;
    ptr[3] = 40;
    ptr[4] = 50;

    println("✓ Wrote 5 values to allocated memory");

    // 3. Read values back
    print("  ptr[0] = ");
    print_int(ptr[0]);
    println("");

    print("  ptr[1] = ");
    print_int(ptr[1]);
    println("");

    print("  ptr[2] = ");
    print_int(ptr[2]);
    println("");

    print("  ptr[3] = ");
    print_int(ptr[3]);
    println("");

    print("  ptr[4] = ");
    print_int(ptr[4]);
    println("");

    // 4. Verify values
    if (ptr[0] != 10 || ptr[1] != 20 || ptr[2] != 30) {
        println("ERROR: Values don't match");
        return 1;
    }

    println("✓ Values verified correctly");

    // 5. Free memory
    free(ptr);
    println("✓ free() called");

    println("");
    println("=== All malloc tests passed! ===");

    return 0;
}
