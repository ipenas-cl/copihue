// Test tokenizing just one character

struct Token {
    type: i64,
    start: i64,
    length: i64,
    line: i64
}

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

fn advance(lex: *Lexer) -> i64 {
    let ch = lex.source[lex.pos];
    lex.pos = lex.pos + 1;
    return ch;
}

fn main() -> i64 {
    println("=== Test One Token ===");

    let source: [i8; 5];
    source[0] = 102;  // 'f'
    source[1] = 110;  // 'n'
    source[2] = 0;

    let lex: Lexer;
    lexer_init(&lex, source);

    let tok: Token;
    tok.type = 0;
    tok.start = lex.pos;
    tok.line = lex.line;

    let ch = advance(&lex);

    print("Got char: ");
    print_int(ch);
    println("");

    // Check if alpha (f = 102, should be 1)
    let is_alpha = 0;
    if (ch >= 97 && ch <= 122) {
        is_alpha = 1;
    }

    print("Is alpha: ");
    print_int(is_alpha);
    println("");

    tok.length = lex.pos;
    tok.type = 1;  // T_IDENT

    print("Token type: ");
    print_int(tok.type);
    println("");
    print("Token length: ");
    print_int(tok.length);
    println("");

    println("✅ Success!");
    return 0;
}
