// Minimal AST test

fn ast_init_fields(ptr: *i64, node_type: i64) -> i64 {
    ptr[0] = node_type;
    ptr[1] = 0;  // name = NULL
    ptr[2] = 0;  // value = 0
    ptr[3] = 0;  // str_value = NULL
    ptr[4] = 0;  // op = NULL
    ptr[5] = 0;  // line = 0
    ptr[6] = 0;  // children = NULL
    ptr[7] = 0;  // child_count = 0
    ptr[8] = 0;  // child_capacity = 0
    return 0;
}

fn ast_new(node_type: i64) -> i64 {
    let size = 80;
    let node_ptr = malloc(size);

    if (node_ptr == 0) {
        println("ERROR: malloc failed!");
        return 0;
    }

    ast_init_fields(node_ptr, node_type);
    return node_ptr;
}

fn ast_get_type(node: *i64) -> i64 {
    return node[0];
}

fn main() -> i64 {
    println("=== Minimal AST Test ===");

    let node = ast_new(9);  // AST_NUMBER

    if (node == 0) {
        println("❌ ast_new failed!");
        return 1;
    }

    print("✅ Created node at: ");
    print_int(node);
    println("");

    let node_type = ast_get_type(node);
    print("node_type = ");
    print_int(node_type);
    println("");

    free(node);

    println("✅ Test complete!");
    return 0;
}
