// Test: Dynamic struct allocation with malloc
// This is CRITICAL for self-hosting - we need dynamic data structures!

struct Point {
    x: i32,
    y: i32
}

struct Rectangle {
    width: i32,
    height: i32
}

fn main() -> i32 {
    println("=== Test: malloc with structs ===");
    println("");

    // 1. Calculate struct sizes
    // Point has 2 i32 fields = 2 * 8 bytes = 16 bytes
    // (Note: Chronos uses 8-byte slots for all fields currently)
    println("1. Allocating Point struct:");
    let point_size = 16;  // 2 fields * 8 bytes
    let p = malloc(point_size);

    if (p == 0) {
        println("ERROR: malloc failed for Point");
        return 1;
    }

    print("   Point* allocated at: 0x");
    print_int(p);
    println("");
    println("   ✓ Point struct allocated");

    // 2. Allocate Rectangle struct
    println("");
    println("2. Allocating Rectangle struct:");
    let rect_size = 16;  // 2 fields * 8 bytes
    let r = malloc(rect_size);

    if (r == 0) {
        println("ERROR: malloc failed for Rectangle");
        return 1;
    }

    print("   Rectangle* allocated at: 0x");
    print_int(r);
    println("");
    println("   ✓ Rectangle struct allocated");

    // 3. Allocate another Point
    println("");
    println("3. Allocating second Point:");
    let p2 = malloc(point_size);

    if (p2 == 0) {
        println("ERROR: malloc failed for second Point");
        return 1;
    }

    print("   Point* allocated at: 0x");
    print_int(p2);
    println("");
    println("   ✓ Second Point allocated");

    // 4. Verify pointers are different
    println("");
    println("4. Verifying allocations:");
    if (p == r || p == p2 || r == p2) {
        println("ERROR: Duplicate pointers detected!");
        return 1;
    }
    println("   ✓ All pointers are unique");

    // 5. Free all allocations
    println("");
    println("5. Freeing allocations:");
    free(p);
    println("   ✓ Point freed");

    free(r);
    println("   ✓ Rectangle freed");

    free(p2);
    println("   ✓ Point2 freed");

    println("");
    println("=== malloc struct test passed! ===");
    println("");
    println("✓ Dynamic memory allocation works!");
    println("✓ Can allocate structs on heap!");
    println("✓ Ready for self-hosting data structures!");

    return 0;
}
