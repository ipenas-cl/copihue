// Simple AST malloc test

fn main() -> i64 {
    println("=== Simple AST Test ===");

    // Allocate space for AstNode struct (80 bytes = 10 fields * 8 bytes)
    let node = malloc(80);

    if (node == 0) {
        println("❌ malloc failed!");
        return 1;
    }

    print("✅ Allocated node at: ");
    print_int(node);
    println("");

    // Initialize node_type field
    let ptr = node;
    ptr[0] = 9;  // AST_NUMBER

    // Initialize value field
    ptr[2] = 42;

    // Read back
    print("node_type = ");
    print_int(ptr[0]);
    println("");

    print("value = ");
    print_int(ptr[2]);
    println("");

    // Free
    free(node);

    println("✅ Test complete!");
    return 0;
}
