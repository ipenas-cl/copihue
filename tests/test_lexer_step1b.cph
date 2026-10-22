// Step 1b: Verify lexer_init

struct Lexer {
    source: *i8,
    pos: i64,
    line: i64,
    col: i64
}

fn lexer_init(lex: *Lexer, source: *i8) -> i64 {
    print("  In lexer_init: source addr = ");
    print_int(source);
    println("");
    print("  In lexer_init: source[0] = ");
    print_int(source[0]);
    println("");

    lex.source = source;
    lex.pos = 0;
    lex.line = 1;
    lex.col = 1;

    print("  After assignment: lex.source = ");
    print_int(lex.source);
    println("");
    print("  After assignment: lex.source[0] = ");
    print_int(lex.source[0]);
    println("");

    return 0;
}

fn main() -> i64 {
    println("=== Lexer Step 1b: Init Debug ===");

    let source: [i8; 10];
    source[0] = 102;  // 'f'
    source[1] = 110;  // 'n'
    source[2] = 0;

    print("source[0] = ");
    print_int(source[0]);
    println("");

    let lex: Lexer;
    lexer_init(&lex, source);

    print("After init: lex.source = ");
    print_int(lex.source);
    println("");
    print("After init: lex.source[0] = ");
    print_int(lex.source[0]);
    println("");

    return 0;
}
