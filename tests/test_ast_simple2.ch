// Simple AST malloc test with proper pointer types

fn test_node(ptr: *i64) -> i64 {
    // Read node_type
    print("node_type = ");
    print_int(ptr[0]);
    println("");

    // Read value  
    print("value = ");
    print_int(ptr[2]);
    println("");

    return 0;
}

fn main() -> i64 {
    println("=== Simple AST Test v2 ===");

    // Allocate space for AstNode struct (80 bytes = 10 fields * 8 bytes)
    let node = malloc(80);

    if (node == 0) {
        println("❌ malloc failed!");
        return 1;
    }

    print("✅ Allocated node at: ");
    print_int(node);
    println("");

    // Initialize by passing to function that takes pointer
    test_node(node);

    // Free
    free(node);

    println("✅ Test complete!");
    return 0;
}
