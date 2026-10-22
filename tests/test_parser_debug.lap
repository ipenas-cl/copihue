// Debug parser

struct Token {
    type: i64,
    value: i64
}

fn main() -> i64 {
    println("=== Parser Debug ===");

    // Create tokens
    let tokens: [Token; 3];
    tokens[0].type = 1;  // T_NUM
    tokens[0].value = 42;
    tokens[1].type = 2;  // T_PLUS
    tokens[2].type = 1;  // T_NUM
    tokens[2].value = 99;

    // Read back
    print("tokens[0].type = ");
    print_int(tokens[0].type);
    println("");

    print("tokens[0].value = ");
    print_int(tokens[0].value);
    println("");

    print("tokens[2].value = ");
    print_int(tokens[2].value);
    println("");

    println("✅ Test complete!");
    return 0;
}
