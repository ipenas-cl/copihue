// Minimal lexer test

let T_EOF = 0;
let T_FN = 4;

struct Token {
    type: i32,
    start: i32,
    length: i32,
    line: i32
}

struct Lexer {
    source: *i8,
    pos: i32,
    line: i32,
    col: i32
}

fn lexer_init(lex: *Lexer, source: *i8) -> i32 {
    lex.source = source;
    lex.pos = 0;
    lex.line = 1;
    lex.col = 1;
    return 0;
}

fn test_token_struct(tok: *Token) -> i32 {
    tok.type = 99;
    tok.start = 10;
    tok.length = 5;
    tok.line = 1;
    return 0;
}

fn main() -> i32 {
    println("=== Minimal Lexer Test ===");

    let tok: Token;
    test_token_struct(&tok);

    print("tok.type = ");
    print_int(tok.type);
    println("");

    print("tok.start = ");
    print_int(tok.start);
    println("");

    print("tok.length = ");
    print_int(tok.length);
    println("");

    print("tok.line = ");
    print_int(tok.line);
    println("");

    return 0;
}
