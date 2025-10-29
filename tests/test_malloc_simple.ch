// Simple malloc test without array indexing

fn main() -> i64 {
    println("=== Simple malloc Test ===");

    // Allocate 100 bytes
    let ptr = malloc(100);

    if (ptr == 0) {
        println("❌ malloc failed!");
        return 1;
    }

    print("✅ malloc succeeded, address: ");
    print_int(ptr);
    println("");

    // Free memory
    let result = free(ptr);

    if (result == 0) {
        println("✅ free succeeded!");
    } else {
        print("❌ free failed with code: ");
        print_int(result);
        println("");
        return 1;
    }

    println("✅ All tests passed!");
    return 0;
}
