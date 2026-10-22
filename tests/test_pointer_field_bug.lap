// Test pointer field access bug

struct Token {
    type: i64,
    value: i64
}

struct Container {
    tokens: *Token,
    count: i64
}

fn test_access(c: *Container) -> i64 {
    print("c.tokens[0].type = ");
    print_int(c.tokens[0].type);
    println("");

    print("c.tokens[0].value = ");
    print_int(c.tokens[0].value);
    println("");

    return 0;
}

fn main() -> i64 {
    println("=== Pointer Field Bug Test ===");

    let tokens: [Token; 2];
    tokens[0].type = 1;
    tokens[0].value = 42;
    tokens[1].type = 2;
    tokens[1].value = 99;

    let container: Container;
    container.tokens = tokens;
    container.count = 2;

    print("Main: container.tokens[0].type = ");
    print_int(container.tokens[0].type);
    println("");

    print("Main: container.tokens[0].value = ");
    print_int(container.tokens[0].value);
    println("");

    test_access(&container);

    println("✅ Test complete!");
    return 0;
}
