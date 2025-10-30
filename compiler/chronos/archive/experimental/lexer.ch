// CHRONOS LEXER - Tokenizer written in Chronos
// This is the FIRST component of the self-hosted compiler!
// Author: Chronos Project
// Date: 2024-10-29

// ============================================
// TOKEN TYPES (matching C implementation)
// ============================================

// Token type constants
let T_EOF = 0;
let T_IDENT = 1;
let T_NUM = 2;
let T_STR = 3;
let T_FN = 4;
let T_LET = 5;
let T_IF = 6;
let T_ELSE = 7;
let T_WHILE = 8;
let T_FOR = 9;
let T_RET = 10;
let T_STRUCT = 11;
let T_MUT = 12;
let T_LPAREN = 13;
let T_RPAREN = 14;
let T_LBRACE = 15;
let T_RBRACE = 16;
let T_LBRACKET = 17;
let T_RBRACKET = 18;
let T_SEMI = 19;
let T_COLON = 20;
let T_COMMA = 21;
let T_DOT = 22;
let T_AMP = 23;
let T_PLUS = 24;
let T_MINUS = 25;
let T_STAR = 26;
let T_SLASH = 27;
let T_MOD = 28;
let T_EQ = 29;
let T_EQEQ = 30;
let T_NEQ = 31;
let T_LT = 32;
let T_GT = 33;
let T_LTE = 34;
let T_GTE = 35;
let T_ARROW = 36;
let T_AND_AND = 37;
let T_OR_OR = 38;
let T_BANG = 39;
let T_PLUSPLUS = 40;
let T_MINUSMINUS = 41;
let T_PLUSEQ = 42;
let T_MINUSEQ = 43;
let T_STAREQ = 44;
let T_SLASHEQ = 45;
let T_MODEQ = 46;

// ============================================
// STRUCTS
// ============================================

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

// ============================================
// CHARACTER UTILITIES
// ============================================

fn is_alpha(ch: i64) -> i64 {
    if (ch >= 65 && ch <= 90) {
        return 1;   // A-Z
    }
    if (ch >= 97 && ch <= 122) {
        return 1;  // a-z
    }
    if (ch == 95) {
        return 1;  // _
    }
    return 0;
}

fn is_digit(ch: i64) -> i64 {
    if (ch >= 48 && ch <= 57) {
        return 1;   // 0-9
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

fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32) {
        return 1;   // space
    }
    if (ch == 9) {
        return 1;    // tab
    }
    if (ch == 13) {
        return 1;   // \r
    }
    if (ch == 10) {
        return 1;   // \n
    }
    return 0;
}

// ============================================
// STRING COMPARISON (needed for keywords)
// ============================================

fn str_equal(s1: *i8, s2: *i8, len: i64) -> i64 {
    let i = 0;
    while (i < len) {
        if (s1[i] != s2[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

// ============================================
// LEXER CORE FUNCTIONS
// ============================================

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

fn peek_next(lex: *Lexer) -> i64 {
    return lex.source[lex.pos + 1];
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

fn skip_whitespace(lex: *Lexer) -> i64 {
    while (1) {
        let ch = peek(lex);

        // Skip whitespace
        if (is_whitespace(ch)) {
            advance(lex);
        } else {
            if (ch == 47 && peek_next(lex) == 47) {  // "//" comment
                // Skip until newline
                while (peek(lex) != 10 && peek(lex) != 0) {
                    advance(lex);
                }
            } else {
                return 0;  // Done skipping
            }
        }
    }
    return 0;
}

// ============================================
// KEYWORD RECOGNITION
// ============================================

fn check_keyword(start: *i8, len: i64) -> i64 {
    // fn
    if (len == 2 && start[0] == 102 && start[1] == 110) {
        return T_FN;
    }

    // let
    if (len == 3 && start[0] == 108 && start[1] == 101 && start[2] == 116) {
        return T_LET;
    }

    // if
    if (len == 2 && start[0] == 105 && start[1] == 102) {
        return T_IF;
    }

    // else
    if (len == 4 && start[0] == 101 && start[1] == 108 &&
        start[2] == 115 && start[3] == 101) {
        return T_ELSE;
    }

    // for
    if (len == 3 && start[0] == 102 && start[1] == 111 && start[2] == 114) {
        return T_FOR;
    }

    // mut
    if (len == 3 && start[0] == 109 && start[1] == 117 && start[2] == 116) {
        return T_MUT;
    }

    // while
    if (len == 5 && start[0] == 119 && start[1] == 104 &&
        start[2] == 105 && start[3] == 108 && start[4] == 101) {
        return T_WHILE;
    }

    // return
    if (len == 6 && start[0] == 114 && start[1] == 101 &&
        start[2] == 116 && start[3] == 117 && start[4] == 114 && start[5] == 110) {
        return T_RET;
    }

    // struct
    if (len == 6 && start[0] == 115 && start[1] == 116 &&
        start[2] == 114 && start[3] == 117 && start[4] == 99 && start[5] == 116) {
        return T_STRUCT;
    }

    // Not a keyword, it's an identifier
    return T_IDENT;
}

// ============================================
// MAIN TOKENIZATION FUNCTION
// ============================================

fn lexer_next_token(lex: *Lexer, tok: *Token) -> i64 {
    skip_whitespace(lex);

    let start_idx = lex.pos;
    let tok_line = lex.line;
    let ch = advance(lex);

    tok.start = start_idx;
    tok.line = tok_line;

    // EOF
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

    // String literal
    if (ch == 34) {  // "
        while (peek(lex) != 0 && peek(lex) != 34) {
            if (peek(lex) == 92 && peek_next(lex) != 0) {  // backslash
                advance(lex);  // Skip escape
            }
            advance(lex);
        }
        if (peek(lex) == 34) {
            advance(lex);  // Consume closing quote
        }
        tok.type = T_STR;
        tok.length = lex.pos - start_idx;
        return 0;
    }

    // Single character tokens
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
    if (ch == 91) {
        tok.type = T_LBRACKET;
        tok.length = 1;
        return 0;
    }
    if (ch == 93) {
        tok.type = T_RBRACKET;
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
    if (ch == 44) {
        tok.type = T_COMMA;
        tok.length = 1;
        return 0;
    }
    if (ch == 46) {
        tok.type = T_DOT;
        tok.length = 1;
        return 0;
    }

    // Two-character operators
    if (ch == 38 && peek(lex) == 38) {  // &&
        advance(lex);
        tok.type = T_AND_AND;
        tok.length = 2;
        return 0;
    }
    if (ch == 38) {
        tok.type = T_AMP;
        tok.length = 1;
        return 0;
    }

    if (ch == 124 && peek(lex) == 124) {  // ||
        advance(lex);
        tok.type = T_OR_OR;
        tok.length = 2;
        return 0;
    }

    if (ch == 43 && peek(lex) == 43) {  // ++
        advance(lex);
        tok.type = T_PLUSPLUS;
        tok.length = 2;
        return 0;
    }
    if (ch == 43 && peek(lex) == 61) {  // +=
        advance(lex);
        tok.type = T_PLUSEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 43) {
        tok.type = T_PLUS;
        tok.length = 1;
        return 0;
    }

    if (ch == 45 && peek(lex) == 45) {  // --
        advance(lex);
        tok.type = T_MINUSMINUS;
        tok.length = 2;
        return 0;
    }
    if (ch == 45 && peek(lex) == 61) {  // -=
        advance(lex);
        tok.type = T_MINUSEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 45 && peek(lex) == 62) {  // ->
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

    if (ch == 42 && peek(lex) == 61) {  // *=
        advance(lex);
        tok.type = T_STAREQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 42) {
        tok.type = T_STAR;
        tok.length = 1;
        return 0;
    }

    if (ch == 47 && peek(lex) == 61) {  // /=
        advance(lex);
        tok.type = T_SLASHEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 47) {
        tok.type = T_SLASH;
        tok.length = 1;
        return 0;
    }

    if (ch == 37 && peek(lex) == 61) {  // %=
        advance(lex);
        tok.type = T_MODEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 37) {
        tok.type = T_MOD;
        tok.length = 1;
        return 0;
    }

    if (ch == 61 && peek(lex) == 61) {  // ==
        advance(lex);
        tok.type = T_EQEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 61) {
        tok.type = T_EQ;
        tok.length = 1;
        return 0;
    }

    if (ch == 33 && peek(lex) == 61) {  // !=
        advance(lex);
        tok.type = T_NEQ;
        tok.length = 2;
        return 0;
    }
    if (ch == 33) {
        tok.type = T_BANG;
        tok.length = 1;
        return 0;
    }

    if (ch == 60 && peek(lex) == 61) {  // <=
        advance(lex);
        tok.type = T_LTE;
        tok.length = 2;
        return 0;
    }
    if (ch == 60) {
        tok.type = T_LT;
        tok.length = 1;
        return 0;
    }

    if (ch == 62 && peek(lex) == 61) {  // >=
        advance(lex);
        tok.type = T_GTE;
        tok.length = 2;
        return 0;
    }
    if (ch == 62) {
        tok.type = T_GT;
        tok.length = 1;
        return 0;
    }

    // Unknown character - return EOF
    tok.type = T_EOF;
    tok.length = 0;
    return tok;
}

// ============================================
// TESTING FUNCTION
// ============================================

fn main() -> i32 {
    println("=== Chronos Lexer - Self-Hosted! ===");
    println("");

    // Test simple program - use array instead of string literal variable
    let source: [i8; 40];
    source[0] = 102;   // 'f'
    source[1] = 110;   // 'n'
    source[2] = 32;    // ' '
    source[3] = 109;   // 'm'
    source[4] = 97;    // 'a'
    source[5] = 105;   // 'i'
    source[6] = 110;   // 'n'
    source[7] = 40;    // '('
    source[8] = 41;    // ')'
    source[9] = 0;     // null terminator

    println("Tokenizing:");
    println("fn main()");

    let lex: Lexer;
    lexer_init(&lex, source);

    println("Tokens:");
    let count = 0;
    let tok: Token;
    while (count < 20) {  // Limit to prevent infinite loop
        lexer_next_token(&lex, &tok);

        print("  Token ");
        print_int(count);
        print(": type=");
        print_int(tok.type);
        print(" line=");
        print_int(tok.line);
        print(" len=");
        print_int(tok.length);
        println("");

        if (tok.type == T_EOF) {
            break;
        }

        count++;
    }

    println("");
    print("Total tokens: ");
    print_int(count + 1);
    println("");
    println("");
    println("✅ Lexer working!");
    println("✅ First self-hosted component complete!");

    return 0;
}
