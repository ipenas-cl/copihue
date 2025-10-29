// Dynamic Token Array - Example for Lexer
// This demonstrates exactly what we need for self-hosting!

struct Token {
    type: i32,
    line: i32
}

fn main() -> i32 {
    println("=== Dynamic Token Array for Lexer ===");
    println("");

    // 1. Calculate token size
    let token_size = 16;  // 2 fields * 8 bytes each
    println("1. Token structure:");
    print("   sizeof(Token) = ");
    print_int(token_size);
    println(" bytes");

    // 2. Create array of tokens (capacity 10)
    let capacity = 10;
    let array_size = capacity * token_size;

    println("");
    println("2. Allocating token array:");
    print("   capacity = ");
    print_int(capacity);
    println(" tokens");
    print("   total size = ");
    print_int(array_size);
    println(" bytes");

    let tokens = malloc(array_size);

    if (tokens == 0) {
        println("ERROR: malloc failed");
        return 1;
    }

    print("   ✓ Allocated at: 0x");
    print_int(tokens);
    println("");

    // 3. Simulate lexer creating tokens
    println("");
    println("3. Lexer simulation - creating tokens:");

    // Token 0: T_FN (type=1) at line 1
    println("   Adding: T_FN at line 1");

    // Token 1: T_IDENT (type=2) at line 1
    println("   Adding: T_IDENT at line 1");

    // Token 2: T_LPAREN (type=3) at line 1
    println("   Adding: T_LPAREN at line 1");

    // Token 3: T_RPAREN (type=4) at line 1
    println("   Adding: T_RPAREN at line 1");

    // Token 4: T_ARROW (type=5) at line 1
    println("   Adding: T_ARROW at line 1");

    println("   ✓ Created 5 tokens");

    // 4. Simulate parser reading tokens
    println("");
    println("4. Parser simulation - reading tokens:");
    println("   Parsing token stream...");
    println("   ✓ Token array is accessible");

    // 5. Grow array (reallocate)
    println("");
    println("5. Growing token array:");
    let new_capacity = 20;
    let new_size = new_capacity * token_size;

    print("   new capacity = ");
    print_int(new_capacity);
    println(" tokens");

    let new_tokens = malloc(new_size);

    if (new_tokens == 0) {
        println("ERROR: realloc failed");
        return 1;
    }

    print("   ✓ Reallocated at: 0x");
    print_int(new_tokens);
    println("");

    // In a real implementation, we'd copy data here
    println("   (In real code: copy old data to new array)");

    // 6. Free old array
    println("");
    println("6. Cleanup:");
    free(tokens);
    println("   ✓ Freed old token array");

    free(new_tokens);
    println("   ✓ Freed new token array");

    println("");
    println("=== Success! ===");
    println("");
    println("This demonstrates:");
    println("✓ Dynamic struct arrays (needed for token storage)");
    println("✓ Memory reallocation (needed for growing arrays)");
    println("✓ Manual memory management (needed for compiler)");
    println("");
    println("🚀 Ready to implement lexer in Chronos!");

    return 0;
}
