// Comprehensive lexer test - all token types

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

// Token types
let T_EOF = 0;
let T_IDENT = 1;
let T_NUM = 2;
let T_FN = 4;
let T_LET = 5;
let T_LPAREN = 13;
let T_RPAREN = 14;
let T_LBRACE = 15;
let T_RBRACE = 16;
let T_SEMI = 19;
let T_COLON = 20;
let T_PLUS = 24;
let T_MINUS = 25;
let T_STAR = 26;
let T_ARROW = 36;

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
    if (ch == 10) {
        lex.line = lex.line + 1;
        lex.col = 1;
    } else {
        lex.col = lex.col + 1;
    }
    return ch;
}

fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32 || ch == 10 || ch == 13 || ch == 9) {
        return 1;
    }
    return 0;
}

fn skip_whitespace(lex: *Lexer) -> i64 {
    while (is_whitespace(peek(lex))) {
        advance(lex);
    }
    return 0;
}

fn is_alpha(ch: i64) -> i64 {
    if (ch >= 97 && ch <= 122) {
        return 1;
    }
    if (ch >= 65 && ch <= 90) {
        return 1;
    }
    if (ch == 95) {
        return 1;
    }
    return 0;
}

fn is_digit(ch: i64) -> i64 {
    if (ch >= 48 && ch <= 57) {
        return 1;
    }
    return 0;
}

fn is_alnum(ch: i64) -> i64 {
    if (is_alpha(ch)) {
        return 1;
    }
    if (is_digit(ch)) {
        return 1;
    }
    return 0;
}

fn check_keyword(start: *i8, len: i64) -> i64 {
    if (len == 2 && start[0] == 102 && start[1] == 110) {
        return T_FN;
    }
    if (len == 3 && start[0] == 108 && start[1] == 101 && start[2] == 116) {
        return T_LET;
    }
    return T_IDENT;
}

fn lexer_next_token(lex: *Lexer, tok: *Token) -> i64 {
    skip_whitespace(lex);

    let start_idx = lex.pos;
    let tok_line = lex.line;
    let ch = advance(lex);

    tok.start = start_idx;
    tok.line = tok_line;

    if (ch == 0) {
        tok.type = T_EOF;
        tok.length = 0;
        return 0;
    }

    // Identifier or keyword
    if (is_alpha(ch)) {
        while (is_alnum(peek(lex))) {
            advance(lex);
        }
        let len = lex.pos - start_idx;
        tok.type = check_keyword(lex.source + start_idx, len);
        tok.length = len;
        return 0;
    }

    // Number
    if (is_digit(ch)) {
        while (is_digit(peek(lex))) {
            advance(lex);
        }
        tok.type = T_NUM;
        tok.length = lex.pos - start_idx;
        return 0;
    }

    // Single-character tokens
    if (ch == 40) {
        tok.type = T_LPAREN;
        tok.length = 1;
        return 0;
    }
    if (ch == 41) {
        tok.type = T_RPAREN;
        tok.length = 1;
        return 0;
    }
    if (ch == 123) {
        tok.type = T_LBRACE;
        tok.length = 1;
        return 0;
    }
    if (ch == 125) {
        tok.type = T_RBRACE;
        tok.length = 1;
        return 0;
    }
    if (ch == 59) {
        tok.type = T_SEMI;
        tok.length = 1;
        return 0;
    }
    if (ch == 58) {
        tok.type = T_COLON;
        tok.length = 1;
        return 0;
    }
    if (ch == 43) {
        tok.type = T_PLUS;
        tok.length = 1;
        return 0;
    }
    if (ch == 42) {
        tok.type = T_STAR;
        tok.length = 1;
        return 0;
    }

    // Arrow ->
    if (ch == 45 && peek(lex) == 62) {
        advance(lex);
        tok.type = T_ARROW;
        tok.length = 2;
        return 0;
    }
    if (ch == 45) {
        tok.type = T_MINUS;
        tok.length = 1;
        return 0;
    }

    tok.type = T_EOF;
    tok.length = 0;
    return 0;
}

fn test_tokens(lex: *Lexer, expected: *i64, count: i64) -> i64 {
    let tok: Token;
    let passed = 0;
    let failed = 0;

    print("  [DEBUG] expected ptr: ");
    print_int(expected);
    print(", expected[0]: ");
    print_int(expected[0]);
    println("");

    let i = 0;
    while (i < count) {
        lexer_next_token(lex, &tok);

        print("  [DEBUG] i=");
        print_int(i);
        print(" expected[i]=");
        print_int(expected[i]);
        print(" tok.type=");
        print_int(tok.type);
        println("");

        if (tok.type == expected[i]) {
            passed = passed + 1;
        } else {
            print("  ❌ Token ");
            print_int(i);
            print(": expected type=");
            print_int(expected[i]);
            print(" got type=");
            print_int(tok.type);
            println("");
            failed = failed + 1;
        }

        i = i + 1;
    }

    print("  Passed: ");
    print_int(passed);
    print("/");
    print_int(count);
    println("");

    return failed;
}

fn main() -> i64 {
    println("=== Comprehensive Lexer Test ===");
    println("");

    // Test 1: Keywords and identifiers
    println("Test 1: Keywords and identifiers");
    let src1: [i8; 20];
    src1[0] = 102;  // f
    src1[1] = 110;  // n
    src1[2] = 32;   // space
    src1[3] = 108;  // l
    src1[4] = 101;  // e
    src1[5] = 116;  // t
    src1[6] = 32;   // space
    src1[7] = 120;  // x
    src1[8] = 0;

    let lex1: Lexer;
    lexer_init(&lex1, src1);

    let expected1: [i64; 3];
    expected1[0] = T_FN;
    expected1[1] = T_LET;
    expected1[2] = T_IDENT;

    print("  Expected values: ");
    print_int(expected1[0]);
    print(", ");
    print_int(expected1[1]);
    print(", ");
    print_int(expected1[2]);
    println("");

    let errors1 = test_tokens(&lex1, expected1, 3);
    println("");

    // Test 2: Numbers and operators
    println("Test 2: Numbers and operators");
    let src2: [i8; 15];
    src2[0] = 52;   // 4
    src2[1] = 50;   // 2
    src2[2] = 32;   // space
    src2[3] = 43;   // +
    src2[4] = 32;   // space
    src2[5] = 49;   // 1
    src2[6] = 0;

    let lex2: Lexer;
    lexer_init(&lex2, src2);

    let expected2: [i64; 3];
    expected2[0] = T_NUM;
    expected2[1] = T_PLUS;
    expected2[2] = T_NUM;

    let errors2 = test_tokens(&lex2, expected2, 3);
    println("");

    // Test 3: Delimiters and arrow
    println("Test 3: Delimiters and arrow");
    let src3: [i8; 20];
    src3[0] = 40;   // (
    src3[1] = 41;   // )
    src3[2] = 32;   // space
    src3[3] = 45;   // -
    src3[4] = 62;   // >
    src3[5] = 32;   // space
    src3[6] = 123;  // {
    src3[7] = 125;  // }
    src3[8] = 0;

    let lex3: Lexer;
    lexer_init(&lex3, src3);

    let expected3: [i64; 4];
    expected3[0] = T_LPAREN;
    expected3[1] = T_RPAREN;
    expected3[2] = T_ARROW;
    expected3[3] = T_LBRACE;

    let errors3 = test_tokens(&lex3, expected3, 4);
    println("");

    // Summary
    let total_errors = errors1 + errors2 + errors3;

    if (total_errors == 0) {
        println("✅ All tests passed!");
    } else {
        print("❌ ");
        print_int(total_errors);
        println(" tests failed");
    }

    return 0;
}
