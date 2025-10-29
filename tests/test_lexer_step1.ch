// Step 1: Test basic struct and pointer access

struct Lexer {
    source: *i8,
    pos: i64,
    line: i64,
    col: i64
}

fn lexer_init(lex: *Lexer, source: *i8) -> i64 {
    lex.source = source;
    lex.pos = 0;
    lex.line = 1;
    lex.col = 1;
    return 0;
}

fn peek(lex: *Lexer) -> i64 {
    return lex.source[lex.pos];
}

fn advance(lex: *Lexer) -> i64 {
    let ch = lex.source[lex.pos];
    lex.pos = lex.pos + 1;
    return ch;
}

fn main() -> i64 {
    println("=== Lexer Step 1: Basic Access ===");

    let source: [i8; 10];
    source[0] = 102;  // 'f'
    source[1] = 110;  // 'n'
    source[2] = 0;

    let lex: Lexer;
    lexer_init(&lex, source);

    print("peek() = ");
    let ch1 = peek(&lex);
    print_int(ch1);
    println("");

    print("advance() = ");
    let ch2 = advance(&lex);
    print_int(ch2);
    println("");

    print("peek() after advance = ");
    let ch3 = peek(&lex);
    print_int(ch3);
    println("");

    println("✅ Step 1 complete!");
    return 0;
}
