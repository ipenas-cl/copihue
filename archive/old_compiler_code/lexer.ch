// Chronos Lexer v1.0
// Tokenizer for the Chronos programming language
// Converts source code text into a stream of tokens

// ==== TOKEN TYPES ====

// Special
let TOK_EOF: i64 = 0;
let TOK_ERROR: i64 = 1;

// Literals
let TOK_NUMBER: i64 = 10;
let TOK_IDENT: i64 = 11;

// Keywords
let TOK_FN: i64 = 20;
let TOK_LET: i64 = 21;
let TOK_RETURN: i64 = 22;
let TOK_IF: i64 = 23;
let TOK_ELSE: i64 = 24;
let TOK_WHILE: i64 = 25;
let TOK_STRUCT: i64 = 26;

// Types
let TOK_I64: i64 = 30;
let TOK_I8: i64 = 31;

// Operators
let TOK_PLUS: i64 = 40;
let TOK_MINUS: i64 = 41;
let TOK_STAR: i64 = 42;
let TOK_SLASH: i64 = 43;
let TOK_EQ: i64 = 44;
let TOK_EQEQ: i64 = 45;
let TOK_NEQ: i64 = 46;
let TOK_LT: i64 = 47;
let TOK_GT: i64 = 48;
let TOK_LTEQ: i64 = 49;
let TOK_GTEQ: i64 = 50;

// Punctuation
let TOK_LPAREN: i64 = 60;
let TOK_RPAREN: i64 = 61;
let TOK_LBRACE: i64 = 62;
let TOK_RBRACE: i64 = 63;
let TOK_LBRACKET: i64 = 64;
let TOK_RBRACKET: i64 = 65;
let TOK_SEMICOLON: i64 = 66;
let TOK_COLON: i64 = 67;
let TOK_COMMA: i64 = 68;
let TOK_DOT: i64 = 69;
let TOK_ARROW: i64 = 70;
let TOK_AMPERSAND: i64 = 71;

// ==== DATA STRUCTURES ====

struct Token {
    type: i64,
    value: *i8,
    line: i64,
    column: i64,
    length: i64
}

struct TokenList {
    tokens: *Token,
    count: i64,
    capacity: i64
}

struct Lexer {
    source: *i8,
    pos: i64,
    line: i64,
    column: i64,
    current_char: i64
}

// ==== HELPER FUNCTIONS ====

fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32) { return 1; }   // space
    if (ch == 9) { return 1; }    // tab
    if (ch == 10) { return 1; }   // newline
    if (ch == 13) { return 1; }   // carriage return
    return 0;
}

fn is_digit(ch: i64) -> i64 {
    if (ch >= 48 && ch <= 57) { return 1; }  // '0' to '9'
    return 0;
}

fn is_alpha(ch: i64) -> i64 {
    if (ch >= 97 && ch <= 122) { return 1; }  // a-z
    if (ch >= 65 && ch <= 90) { return 1; }   // A-Z
    if (ch == 95) { return 1; }               // underscore
    return 0;
}

fn str_equals(s1: *i8, s2: *i8) -> i64 {
    let i: i64 = 0;
    while (s1[i] != 0 && s2[i] != 0) {
        if (s1[i] != s2[i]) {
            return 0;
        }
        i = i + 1;
    }
    return s1[i] == s2[i];
}

// ==== TOKEN CONSTRUCTION ====

fn make_token(tok_type: i64, value: *i8, line: i64, column: i64, length: i64) -> i64 {
    let tok: *Token = malloc(40);
    tok.type = tok_type;
    tok.value = value;
    tok.line = line;
    tok.column = column;
    tok.length = length;
    return tok;
}

// ==== LEXER STATE ====

fn lexer_init(source: *i8) -> i64 {
    let lex: *Lexer = malloc(32);
    lex.source = source;
    lex.pos = 0;
    lex.line = 1;
    lex.column = 1;

    if (source[0] == 0) {
        lex.current_char = -1;  // EOF
    } else {
        lex.current_char = source[0];
    }

    return lex;
}

fn lexer_advance(lex: *Lexer) -> i64 {
    if (lex.current_char == 10) {  // newline
        lex.line = lex.line + 1;
        lex.column = 1;
    } else {
        lex.column = lex.column + 1;
    }

    lex.pos = lex.pos + 1;

    if (lex.source[lex.pos] == 0) {
        lex.current_char = -1;  // EOF
    } else {
        lex.current_char = lex.source[lex.pos];
    }

    return lex.current_char;
}

fn lexer_peek(lex: *Lexer, ahead: i64) -> i64 {
    let peek_pos: i64 = lex.pos + ahead;
    if (lex.source[peek_pos] == 0) {
        return -1;  // EOF
    }
    return lex.source[peek_pos];
}

// ==== KEYWORD DETECTION ====

fn keyword_or_ident(word: *i8) -> i64 {
    if (str_equals(word, "fn")) { return TOK_FN; }
    if (str_equals(word, "let")) { return TOK_LET; }
    if (str_equals(word, "return")) { return TOK_RETURN; }
    if (str_equals(word, "if")) { return TOK_IF; }
    if (str_equals(word, "else")) { return TOK_ELSE; }
    if (str_equals(word, "while")) { return TOK_WHILE; }
    if (str_equals(word, "struct")) { return TOK_STRUCT; }
    if (str_equals(word, "i64")) { return TOK_I64; }
    if (str_equals(word, "i8")) { return TOK_I8; }

    return TOK_IDENT;
}

// ==== NUMBER LEXING ====

fn lex_number(lex: *Lexer, start_line: i64, start_column: i64) -> i64 {
    let start_pos: i64 = lex.pos;
    let digit_count: i64 = 0;
    let max_digits: i64 = 19;  // Security: prevent overflow

    while (is_digit(lex.current_char) == 1 && digit_count < max_digits) {
        lexer_advance(lex);
        digit_count = digit_count + 1;
    }

    let length: i64 = lex.pos - start_pos;
    let value: *i8 = malloc(length + 1);
    let i: i64 = 0;
    while (i < length) {
        value[i] = lex.source[start_pos + i];
        i = i + 1;
    }
    value[length] = 0;

    return make_token(TOK_NUMBER, value, start_line, start_column, length);
}

// ==== IDENTIFIER LEXING ====

fn lex_identifier(lex: *Lexer, start_line: i64, start_column: i64) -> i64 {
    let start_pos: i64 = lex.pos;

    lexer_advance(lex);

    while (is_alpha(lex.current_char) == 1 || is_digit(lex.current_char) == 1) {
        lexer_advance(lex);
    }

    let length: i64 = lex.pos - start_pos;
    let value: *i8 = malloc(length + 1);
    let i: i64 = 0;
    while (i < length) {
        value[i] = lex.source[start_pos + i];
        i = i + 1;
    }
    value[length] = 0;

    let token_type: i64 = keyword_or_ident(value);

    return make_token(token_type, value, start_line, start_column, length);
}

// ==== MAIN LEXER FUNCTION ====

fn lex_next(lex: *Lexer) -> i64 {
    // Skip whitespace
    while (is_whitespace(lex.current_char) == 1) {
        lexer_advance(lex);
    }

    // EOF
    if (lex.current_char == -1) {
        return make_token(TOK_EOF, "", lex.line, lex.column, 0);
    }

    let start_line: i64 = lex.line;
    let start_column: i64 = lex.column;

    // Numbers
    if (is_digit(lex.current_char) == 1) {
        return lex_number(lex, start_line, start_column);
    }

    // Identifiers and keywords
    if (is_alpha(lex.current_char) == 1) {
        return lex_identifier(lex, start_line, start_column);
    }

    let ch: i64 = lex.current_char;

    // Two-character operators
    if (ch == 61) {  // '='
        if (lexer_peek(lex, 1) == 61) {  // "=="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_EQEQ, "==", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_EQ, "=", start_line, start_column, 1);
    }

    if (ch == 33) {  // '!'
        if (lexer_peek(lex, 1) == 61) {  // "!="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_NEQ, "!=", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_ERROR, "!", start_line, start_column, 1);
    }

    if (ch == 60) {  // '<'
        if (lexer_peek(lex, 1) == 61) {  // "<="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_LTEQ, "<=", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_LT, "<", start_line, start_column, 1);
    }

    if (ch == 62) {  // '>'
        if (lexer_peek(lex, 1) == 61) {  // ">="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_GTEQ, ">=", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_GT, ">", start_line, start_column, 1);
    }

    if (ch == 45) {  // '-'
        if (lexer_peek(lex, 1) == 62) {  // "->"
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_ARROW, "->", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_MINUS, "-", start_line, start_column, 1);
    }

    // Single-character tokens
    if (ch == 43) {  // '+'
        lexer_advance(lex);
        return make_token(TOK_PLUS, "+", start_line, start_column, 1);
    }
    if (ch == 42) {  // '*'
        lexer_advance(lex);
        return make_token(TOK_STAR, "*", start_line, start_column, 1);
    }
    if (ch == 47) {  // '/'
        lexer_advance(lex);
        return make_token(TOK_SLASH, "/", start_line, start_column, 1);
    }
    if (ch == 40) {  // '('
        lexer_advance(lex);
        return make_token(TOK_LPAREN, "(", start_line, start_column, 1);
    }
    if (ch == 41) {  // ')'
        lexer_advance(lex);
        return make_token(TOK_RPAREN, ")", start_line, start_column, 1);
    }
    if (ch == 123) {  // '{'
        lexer_advance(lex);
        return make_token(TOK_LBRACE, "{", start_line, start_column, 1);
    }
    if (ch == 125) {  // '}'
        lexer_advance(lex);
        return make_token(TOK_RBRACE, "}", start_line, start_column, 1);
    }
    if (ch == 91) {  // '['
        lexer_advance(lex);
        return make_token(TOK_LBRACKET, "[", start_line, start_column, 1);
    }
    if (ch == 93) {  // ']'
        lexer_advance(lex);
        return make_token(TOK_RBRACKET, "]", start_line, start_column, 1);
    }
    if (ch == 59) {  // ';'
        lexer_advance(lex);
        return make_token(TOK_SEMICOLON, ";", start_line, start_column, 1);
    }
    if (ch == 58) {  // ':'
        lexer_advance(lex);
        return make_token(TOK_COLON, ":", start_line, start_column, 1);
    }
    if (ch == 44) {  // ','
        lexer_advance(lex);
        return make_token(TOK_COMMA, ",", start_line, start_column, 1);
    }
    if (ch == 46) {  // '.'
        lexer_advance(lex);
        return make_token(TOK_DOT, ".", start_line, start_column, 1);
    }
    if (ch == 38) {  // '&'
        lexer_advance(lex);
        return make_token(TOK_AMPERSAND, "&", start_line, start_column, 1);
    }

    // Unknown character, error
    lexer_advance(lex);
    let error_buf: *i8 = malloc(2);
    error_buf[0] = ch;
    error_buf[1] = 0;
    return make_token(TOK_ERROR, error_buf, start_line, start_column, 1);
}

// ==== TOKEN LIST ====

fn tokenlist_init() -> i64 {
    let list: *TokenList = malloc(24);
    list.capacity = 256;
    list.count = 0;
    list.tokens = malloc(list.capacity * 40);
    return list;
}

fn tokenlist_add(list: *TokenList, tok: *Token) -> i64 {
    if (list.count >= list.capacity) {
        println("ERROR: Too many tokens (max 256)");
        return 1;
    }

    let offset: i64 = list.count * 40;
    let dest: *Token = list.tokens + offset;
    dest.type = tok.type;
    dest.value = tok.value;
    dest.line = tok.line;
    dest.column = tok.column;
    dest.length = tok.length;

    list.count = list.count + 1;
    return 0;
}

// ==== TOKENIZE ENTIRE SOURCE ====

fn lex_all(source: *i8) -> i64 {
    let lex: *Lexer = lexer_init(source);
    let list: *TokenList = tokenlist_init();

    let done: i64 = 0;
    while (done == 0) {
        let tok: *Token = lex_next(lex);
        tokenlist_add(list, tok);

        if (tok.type == TOK_EOF) {
            done = 1;
        }
        if (tok.type == TOK_ERROR) {
            print("Lexical error at line ");
            print_int(tok.line);
            print(": unexpected character '");
            print(tok.value);
            println("'");
            done = 1;
        }
    }

    return list;
}

// ==== TEST/DEMO MAIN ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS LEXER v1.0");
    println("========================================");
    println("");

    // Test simple tokenization first
    let source1: *i8 = "let x: i64 = 42;";

    println("Source:");
    println(source1);
    println("");

    let lex: *Lexer = lexer_init(source1);

    println("Tokenizing...");
    let count: i64 = 0;
    let done: i64 = 0;

    while (done == 0 && count < 20) {
        let tok: *Token = lex_next(lex);

        print("  [");
        print_int(tok.type);
        print("] len=");
        print_int(tok.length);
        print(" val=");

        let j: i64 = 0;
        while (j < tok.length && j < 10) {
            print_int(tok.value[j]);
            print(" ");
            j = j + 1;
        }
        println("");

        if (tok.type == TOK_EOF) {
            done = 1;
        }
        if (tok.type == TOK_ERROR) {
            done = 1;
        }

        count = count + 1;
    }

    println("");
    println("✅ Lexer test complete!");

    return 0;
}
