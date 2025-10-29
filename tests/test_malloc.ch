// Test malloc and free

fn main() -> i64 {
    println("=== malloc/free Test ===");

    // Allocate 100 bytes
    print("Allocating 100 bytes...");
    let ptr = malloc(100);
    println("");

    if (ptr == 0) {
        println("❌ malloc failed!");
        return 1;
    }

    print("✅ Allocated at address: ");
    print_int(ptr);
    println("");

    // Write some data
    println("Writing data...");
    ptr[0] = 42;
    ptr[1] = 99;
    ptr[2] = 123;

    // Read back
    print("ptr[0] = ");
    print_int(ptr[0]);
    println("");

    print("ptr[1] = ");
    print_int(ptr[1]);
    println("");

    print("ptr[2] = ");
    print_int(ptr[2]);
    println("");

    // Free memory
    println("Freeing memory...");
    let result = free(ptr);

    if (result == 0) {
        println("✅ free succeeded!");
    } else {
        println("❌ free failed!");
        return 1;
    }

    println("✅ All tests passed!");
    return 0;
}
