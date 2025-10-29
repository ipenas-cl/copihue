// Debug test to see lexer state

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

fn peek(lex: *Lexer) -> i64 {
    return lex.source[lex.pos];
}

fn advance(lex: *Lexer) -> i64 {
    let ch = lex.source[lex.pos];
    lex.pos = lex.pos + 1;

    if (ch == 10) {  // newline
        lex.line = lex.line + 1;
        lex.col = 1;
    } else {
        lex.col = lex.col + 1;
    }

    return ch;
}

fn is_alpha(ch: i64) -> i64 {
    if (ch >= 97 && ch <= 122) {
        return 1;  // lowercase
    }
    if (ch >= 65 && ch <= 90) {
        return 1;  // uppercase
    }
    if (ch == 95) {
        return 1;  // underscore
    }
    return 0;
}

fn is_alnum(ch: i64) -> i64 {
    if (is_alpha(ch)) {
        return 1;
    }
    if (ch >= 48 && ch <= 57) {
        return 1;  // digit
    }
    return 0;
}

fn main() -> i64 {
    println("=== Lexer Debug Test ===");

    // "fn main"
    let source: [i8; 10];
    source[0] = 102;   // 'f'
    source[1] = 110;   // 'n'
    source[2] = 32;    // ' '
    source[3] = 109;   // 'm'
    source[4] = 97;    // 'a'
    source[5] = 105;   // 'i'
    source[6] = 110;   // 'n'
    source[7] = 0;

    let lex: Lexer;
    lexer_init(&lex, source);

    print("Initial pos: ");
    print_int(lex.pos);
    println("");

    // Manually tokenize first identifier "fn"
    let start = lex.pos;
    let ch = advance(&lex);

    print("After first advance: ch=");
    print_int(ch);
    print(" pos=");
    print_int(lex.pos);
    println("");

    // Check if alpha
    if (is_alpha(ch)) {
        println("Is alpha!");

        // Consume rest of identifier
        while (is_alnum(peek(&lex))) {
            print("  Peeking: ch=");
            print_int(peek(&lex));
            println("");
            advance(&lex);
            print("  After advance: pos=");
            print_int(lex.pos);
            println("");
        }

        let len = lex.pos - start;
        print("Token length: ");
        print_int(len);
        println("");
    }

    print("After first token: pos=");
    print_int(lex.pos);
    println("");

    // Skip whitespace
    while (peek(&lex) == 32 || peek(&lex) == 10) {
        advance(&lex);
    }

    print("After skip whitespace: pos=");
    print_int(lex.pos);
    print(" ch=");
    print_int(peek(&lex));
    println("");

    return 0;
}
