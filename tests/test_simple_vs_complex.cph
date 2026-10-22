// Compare simple vs complex field access

struct Token {
    type: i64,
    value: i64
}

fn main() -> i64 {
    println("=== Simple vs Complex Test ===");

    // Simple case: works
    let tokens: [Token; 2];
    tokens[0].type = 1;
    tokens[0].value = 42;

    print("Simple: tokens[0].value = ");
    print_int(tokens[0].value);
    println("");

    // Now store as pointer
    let ptr: *Token = tokens;
    
    print("Pointer: ptr[0].value = ");
    print_int(ptr[0].value);
    println("");

    println("✅ Test complete!");
    return 0;
}
