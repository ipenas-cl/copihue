// Test pointer type syntax

fn main() -> i64 {
    println("=== Pointer Type Test ===");

    // Use type syntax: let ptr: *i64 = malloc(...)
    let ptr: *i64 = malloc(80);

    if (ptr == 0) {
        println("❌ malloc failed!");
        return 1;
    }

    print("✅ Allocated at: ");
    print_int(ptr);
    println("");

    // Write values
    println("Writing values...");
    ptr[0] = 42;
    ptr[1] = 99;
    ptr[2] = 123;

    // Read values
    print("ptr[0] = ");
    print_int(ptr[0]);
    println("");

    print("ptr[1] = ");
    print_int(ptr[1]);
    println("");

    print("ptr[2] = ");
    print_int(ptr[2]);
    println("");

    free(ptr);

    println("✅ Test complete!");
    return 0;
}
