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
// Chronos AST v1.0
// Abstract Syntax Tree for Chronos programming language
// Recursive tree structure for representing programs

// ==== NODE TYPES ====

// Literals
let NODE_NUMBER: i64 = 1;
let NODE_STRING: i64 = 2;
let NODE_IDENT: i64 = 3;

// Binary Operations
let NODE_ADD: i64 = 10;
let NODE_SUB: i64 = 11;
let NODE_MUL: i64 = 12;
let NODE_DIV: i64 = 13;

// Comparison Operations
let NODE_EQ: i64 = 20;
let NODE_NEQ: i64 = 21;
let NODE_LT: i64 = 22;
let NODE_GT: i64 = 23;
let NODE_LTEQ: i64 = 24;
let NODE_GTEQ: i64 = 25;

// Unary Operations
let NODE_NEG: i64 = 30;
let NODE_DEREF: i64 = 31;
let NODE_ADDR: i64 = 32;

// Statements
let NODE_LET: i64 = 40;
let NODE_ASSIGN: i64 = 41;
let NODE_RETURN: i64 = 42;
let NODE_IF: i64 = 43;
let NODE_WHILE: i64 = 44;
let NODE_BLOCK: i64 = 45;
let NODE_EXPR_STMT: i64 = 46;

// Declarations
let NODE_FUNCTION: i64 = 50;
let NODE_PARAM: i64 = 51;
let NODE_STRUCT: i64 = 52;
let NODE_FIELD: i64 = 53;

// Complex Expressions
let NODE_CALL: i64 = 60;
let NODE_INDEX: i64 = 61;
let NODE_FIELD_ACCESS: i64 = 62;

// Program
let NODE_PROGRAM: i64 = 70;

// ==== AST NODE STRUCTURE ====

struct ASTNode {
    node_type: i64,

    // For binary operations and complex nodes
    left: *ASTNode,
    right: *ASTNode,

    // For control flow
    condition: *ASTNode,
    body: *ASTNode,
    else_body: *ASTNode,

    // For values
    value: i64,
    name: *i8,

    // For type information
    type_name: *i8,

    // For lists (children nodes)
    children: *ASTNode,
    next: *ASTNode,

    // Source location
    line: i64,
    column: i64
}

// ==== NODE CONSTRUCTION ====

fn ast_new(node_type: i64) -> i64 {
    let node: *ASTNode = malloc(128);  // 16 fields * 8 bytes

    // Initialize all fields to 0 first
    node.left = 0;
    node.right = 0;
    node.condition = 0;
    node.body = 0;
    node.else_body = 0;
    node.value = 0;
    node.name = 0;
    node.type_name = 0;
    node.children = 0;
    node.next = 0;
    node.line = 0;
    node.column = 0;

    // Set node_type last
    node.node_type = node_type;

    return node;
}

fn ast_number(value: i64) -> i64 {
    let node: *ASTNode = ast_new(NODE_NUMBER);
    node.value = value;
    return node;
}

fn ast_ident(name: *i8) -> i64 {
    let node: *ASTNode = ast_new(NODE_IDENT);
    node.name = name;
    return node;
}

fn ast_binary_op(op: i64, left: *ASTNode, right: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(op);
    node.left = left;
    node.right = right;
    return node;
}

fn ast_unary_op(op: i64, operand: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(op);
    node.left = operand;
    return node;
}

fn ast_let(name: *i8, type_name: *i8, init: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_LET);
    node.name = name;
    node.type_name = type_name;
    node.right = init;
    return node;
}

fn ast_assign(target: *ASTNode, value: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_ASSIGN);
    node.left = target;
    node.right = value;
    return node;
}

fn ast_return(expr: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_RETURN);
    node.left = expr;
    return node;
}

fn ast_if(condition: *ASTNode, then_body: *ASTNode, else_body: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_IF);
    node.condition = condition;
    node.body = then_body;
    node.else_body = else_body;
    return node;
}

fn ast_while(condition: *ASTNode, body: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_WHILE);
    node.condition = condition;
    node.body = body;
    return node;
}

fn ast_block() -> i64 {
    let node: *ASTNode = ast_new(NODE_BLOCK);
    return node;
}

fn ast_function(name: *i8) -> i64 {
    let node: *ASTNode = ast_new(NODE_FUNCTION);
    node.name = name;
    return node;
}

fn ast_call(func_name: *i8) -> i64 {
    let node: *ASTNode = ast_new(NODE_CALL);
    node.name = func_name;
    return node;
}

fn ast_program() -> i64 {
    let node: *ASTNode = ast_new(NODE_PROGRAM);
    return node;
}

// ==== NODE LIST MANAGEMENT ====

fn ast_add_child(parent: *ASTNode, child: *ASTNode) -> i64 {
    if (parent == 0 || child == 0) {
        return 1;
    }

    if (parent.children == 0) {
        parent.children = child;
        return 0;
    }

    // Find last child
    let current: *ASTNode = parent.children;
    let max_iterations: i64 = 1000;  // Security: prevent infinite loop
    let count: i64 = 0;

    while (current.next != 0 && count < max_iterations) {
        current = current.next;
        count = count + 1;
    }
    current.next = child;
    return 0;
}

fn ast_child_count(parent: *ASTNode) -> i64 {
    if (parent == 0) {
        return 0;
    }

    let count: i64 = 0;
    let current: *ASTNode = parent.children;
    let max_iterations: i64 = 1000;  // Security: prevent infinite loop

    while (current != 0 && count < max_iterations) {
        count = count + 1;
        current = current.next;
    }
    return count;
}

// ==== AST PRINTING (for debugging) ====

fn print_indent(level: i64) -> i64 {
    let i: i64 = 0;
    while (i < level) {
        print("  ");
        i = i + 1;
    }
    return 0;
}

fn ast_print_node(node: *ASTNode, level: i64) -> i64 {
    if (node == 0) {
        print_indent(level);
        println("(null)");
        return 0;
    }

    print_indent(level);

    if (node.node_type == NODE_NUMBER) {
        print("NUMBER(");
        print_int(node.value);
        println(")");
    }

    if (node.node_type == NODE_IDENT) {
        print("IDENT(");
        print(node.name);
        println(")");
    }

    if (node.node_type == NODE_ADD) {
        println("ADD");
        ast_print_node(node.left, level + 1);
        ast_print_node(node.right, level + 1);
    }

    if (node.node_type == NODE_SUB) {
        println("SUB");
        ast_print_node(node.left, level + 1);
        ast_print_node(node.right, level + 1);
    }

    if (node.node_type == NODE_MUL) {
        println("MUL");
        ast_print_node(node.left, level + 1);
        ast_print_node(node.right, level + 1);
    }

    if (node.node_type == NODE_DIV) {
        println("DIV");
        ast_print_node(node.left, level + 1);
        ast_print_node(node.right, level + 1);
    }

    if (node.node_type == NODE_RETURN) {
        println("RETURN");
        ast_print_node(node.left, level + 1);
    }

    if (node.node_type == NODE_BLOCK) {
        println("BLOCK");
        let child: *ASTNode = node.children;
        while (child != 0) {
            ast_print_node(child, level + 1);
            child = child.next;
        }
    }

    if (node.node_type == NODE_FUNCTION) {
        print("FUNCTION(");
        print(node.name);
        println(")");
        ast_print_node(node.body, level + 1);
    }

    return 0;
}

fn ast_print(node: *ASTNode) -> i64 {
    println("========================================");
    println("  AST DUMP");
    println("========================================");
    ast_print_node(node, 0);
    println("========================================");
    return 0;
}

// ==== AST EVALUATION (for simple expressions) ====

fn ast_eval(node: *ASTNode) -> i64 {
    if (node == 0) {
        return 0;
    }

    if (node.node_type == NODE_NUMBER) {
        return node.value;
    }

    if (node.node_type == NODE_ADD) {
        return ast_eval(node.left) + ast_eval(node.right);
    }

    if (node.node_type == NODE_SUB) {
        return ast_eval(node.left) - ast_eval(node.right);
    }

    if (node.node_type == NODE_MUL) {
        return ast_eval(node.left) * ast_eval(node.right);
    }

    if (node.node_type == NODE_DIV) {
        let divisor: i64 = ast_eval(node.right);
        if (divisor == 0) {
            println("ERROR: Division by zero");
            return 0;
        }
        return ast_eval(node.left) / divisor;
    }

    if (node.node_type == NODE_NEG) {
        return 0 - ast_eval(node.left);
    }

    println("ERROR: Cannot evaluate node type");
    return 0;
}

// ==== TEST/DEMO MAIN ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS AST v1.0");
    println("========================================");
    println("");

    // Test 1: Simple number
    println("Test 1: Simple number");
    let n1: *ASTNode = ast_number(42);
    ast_print(n1);
    print("Eval: ");
    print_int(ast_eval(n1));
    println("");
    println("");

    // Test 2: Binary operation (10 + 20)
    println("Test 2: Binary operation (10 + 20)");
    let left2: *ASTNode = ast_number(10);
    let right2: *ASTNode = ast_number(20);
    let n2: *ASTNode = ast_binary_op(NODE_ADD, left2, right2);
    ast_print(n2);
    print("Eval: ");
    print_int(ast_eval(n2));
    println("");
    println("");

    // Test 3: Nested operation ((10 + 20) * 3)
    println("Test 3: Nested operation ((10 + 20) * 3)");
    let left3a: *ASTNode = ast_number(10);
    let right3a: *ASTNode = ast_number(20);
    let add: *ASTNode = ast_binary_op(NODE_ADD, left3a, right3a);
    let right3b: *ASTNode = ast_number(3);
    let n3: *ASTNode = ast_binary_op(NODE_MUL, add, right3b);
    ast_print(n3);
    print("Eval: ");
    print_int(ast_eval(n3));
    println("");
    println("");

    // Test 4: Complex expression (2 + 3 * 4)
    println("Test 4: Complex expression (2 + 3 * 4)");
    let left4a: *ASTNode = ast_number(3);
    let right4a: *ASTNode = ast_number(4);
    let mul: *ASTNode = ast_binary_op(NODE_MUL, left4a, right4a);
    let left4b: *ASTNode = ast_number(2);
    let n4: *ASTNode = ast_binary_op(NODE_ADD, left4b, mul);
    ast_print(n4);
    print("Eval: ");
    print_int(ast_eval(n4));
    println("");
    println("");

    // Test 5: Function with return
    println("Test 5: Function with return");
    let func: *ASTNode = ast_function("test");
    let ret: *ASTNode = ast_return(ast_number(42));
    let block: *ASTNode = ast_block();
    ast_add_child(block, ret);
    func.body = block;
    ast_print(func);
    println("");

    // Test 6: Block with multiple statements
    println("Test 6: Block with multiple statements");
    let block2: *ASTNode = ast_block();

    let num10: *ASTNode = ast_number(10);
    let ret1: *ASTNode = ast_return(num10);
    ast_add_child(block2, ret1);

    let num20: *ASTNode = ast_number(20);
    let ret2: *ASTNode = ast_return(num20);
    ast_add_child(block2, ret2);

    let num30: *ASTNode = ast_number(30);
    let ret3: *ASTNode = ast_return(num30);
    ast_add_child(block2, ret3);

    ast_print(block2);
    print("Child count: ");
    print_int(ast_child_count(block2));
    println("");
    println("");

    println("✅ AST tests complete!");

    return 0;
}
// Chronos Parser v1.0
// Recursive Descent Parser with Precedence Climbing
// Converts tokens from lexer into AST

// ==== PARSER STATE ====

struct Parser {
    tokens: *TokenList,
    pos: i64,
    current: *Token,
    had_error: i64
}

// ==== PARSER INITIALIZATION ====

fn parser_init(tokens: *TokenList) -> i64 {
    let p: *Parser = malloc(32);
    p.tokens = tokens;
    p.pos = 0;
    p.had_error = 0;

    // Set current token
    if (tokens.count > 0) {
        p.current = tokens.tokens;
    } else {
        p.current = 0;
    }

    return p;
}

// ==== TOKEN NAVIGATION ====

fn parser_peek(p: *Parser) -> i64 {
    if (p.current == 0) {
        return TOK_EOF;
    }
    return p.current.type;
}

fn parser_advance(p: *Parser) -> i64 {
    if (p.pos >= p.tokens.count) {
        return 0;
    }

    let current_tok: *Token = p.current;
    p.pos = p.pos + 1;

    if (p.pos < p.tokens.count) {
        let offset: i64 = p.pos * 40;  // Token size = 40 bytes
        p.current = p.tokens.tokens + offset;
    } else {
        p.current = 0;
    }

    return current_tok;
}

fn parser_check(p: *Parser, type: i64) -> i64 {
    return parser_peek(p) == type;
}

fn parser_match(p: *Parser, type: i64) -> i64 {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

fn parser_expect(p: *Parser, type: i64) -> i64 {
    if (parser_check(p, type)) {
        return parser_advance(p);
    }

    print("Parse error: expected token type ");
    print_int(type);
    print(", got ");
    print_int(parser_peek(p));
    println("");
    p.had_error = 1;
    return 0;
}

// ==== EXPRESSION PARSING ====

// Forward declarations (workaround for bootstrap compiler)
fn parse_primary(p: *Parser) -> i64;
fn parse_expression(p: *Parser) -> i64;

// Parse primary expression (numbers, identifiers, parentheses)
fn parse_primary(p: *Parser) -> i64 {
    let tok_type: i64 = parser_peek(p);

    // Number literal
    if (tok_type == TOK_NUMBER) {
        let tok: *Token = parser_advance(p);
        // Convert token value (string) to number
        let value: i64 = str_to_int(tok.value);
        return ast_number(value);
    }

    // Identifier
    if (tok_type == TOK_IDENT) {
        let tok: *Token = parser_advance(p);
        return ast_ident(tok.value);
    }

    // Parenthesized expression
    if (tok_type == TOK_LPAREN) {
        parser_advance(p);
        let expr: *ASTNode = parse_expression(p);
        parser_expect(p, TOK_RPAREN);
        return expr;
    }

    // Unary operators
    if (tok_type == TOK_MINUS) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_NEG, operand);
    }

    if (tok_type == TOK_STAR) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_DEREF, operand);
    }

    if (tok_type == TOK_AMPERSAND) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_ADDR, operand);
    }

    print("Parse error: unexpected token ");
    print_int(tok_type);
    println("");
    p.had_error = 1;
    return 0;
}

// Helper: convert string to integer
fn str_to_int(s: *i8) -> i64 {
    let result: i64 = 0;
    let i: i64 = 0;
    let max_digits: i64 = 19;  // Security: prevent overflow

    while (s[i] != 0 && i < max_digits) {
        let digit: i64 = s[i] - 48;  // '0' = 48
        if (digit >= 0 && digit <= 9) {
            result = result * 10;
            result = result + digit;
        }
        i = i + 1;
    }

    return result;
}

// Parse binary expression with precedence
fn parse_binary_expr(p: *Parser, min_prec: i64) -> i64 {
    let left: *ASTNode = parse_primary(p);

    let done: i64 = 0;
    let max_iterations: i64 = 100;  // Security
    let iterations: i64 = 0;

    while (done == 0 && iterations < max_iterations) {
        let tok_type: i64 = parser_peek(p);
        let prec: i64 = get_precedence(tok_type);

        if (prec < min_prec) {
            done = 1;
        } else {
            let op: i64 = tok_type;
            parser_advance(p);

            let right: *ASTNode = parse_binary_expr(p, prec + 1);

            // Convert token type to AST node type
            let node_type: i64 = token_to_node_type(op);
            left = ast_binary_op(node_type, left, right);
        }

        iterations = iterations + 1;
    }

    return left;
}

fn parse_expression(p: *Parser) -> i64 {
    return parse_binary_expr(p, 0);
}

// Get operator precedence
fn get_precedence(tok_type: i64) -> i64 {
    // Comparison (lowest)
    if (tok_type == TOK_EQEQ) { return 1; }
    if (tok_type == TOK_NEQ) { return 1; }
    if (tok_type == TOK_LT) { return 1; }
    if (tok_type == TOK_GT) { return 1; }
    if (tok_type == TOK_LTEQ) { return 1; }
    if (tok_type == TOK_GTEQ) { return 1; }

    // Addition/Subtraction
    if (tok_type == TOK_PLUS) { return 2; }
    if (tok_type == TOK_MINUS) { return 2; }

    // Multiplication/Division (highest)
    if (tok_type == TOK_STAR) { return 3; }
    if (tok_type == TOK_SLASH) { return 3; }

    return 0;  // Not an operator
}

// Convert token type to AST node type
fn token_to_node_type(tok_type: i64) -> i64 {
    if (tok_type == TOK_PLUS) { return NODE_ADD; }
    if (tok_type == TOK_MINUS) { return NODE_SUB; }
    if (tok_type == TOK_STAR) { return NODE_MUL; }
    if (tok_type == TOK_SLASH) { return NODE_DIV; }
    if (tok_type == TOK_EQEQ) { return NODE_EQ; }
    if (tok_type == TOK_NEQ) { return NODE_NEQ; }
    if (tok_type == TOK_LT) { return NODE_LT; }
    if (tok_type == TOK_GT) { return NODE_GT; }
    if (tok_type == TOK_LTEQ) { return NODE_LTEQ; }
    if (tok_type == TOK_GTEQ) { return NODE_GTEQ; }
    return 0;
}

// ==== STATEMENT PARSING ====

fn parse_return_stmt(p: *Parser) -> i64 {
    parser_expect(p, TOK_RETURN);
    let expr: *ASTNode = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON);
    return ast_return(expr);
}

fn parse_let_stmt(p: *Parser) -> i64 {
    parser_expect(p, TOK_LET);

    let name_tok: *Token = parser_expect(p, TOK_IDENT);
    let name: *i8 = name_tok.value;

    let type_name: *i8 = 0;
    if (parser_match(p, TOK_COLON)) {
        // Optional type annotation
        let type_tok: *Token = parser_advance(p);
        type_name = type_tok.value;
    }

    let init: *ASTNode = 0;
    if (parser_match(p, TOK_EQ)) {
        init = parse_expression(p);
    }

    parser_expect(p, TOK_SEMICOLON);
    return ast_let(name, type_name, init);
}

fn parse_expr_stmt(p: *Parser) -> i64 {
    let expr: *ASTNode = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON);
    let stmt: *ASTNode = ast_new(NODE_EXPR_STMT);
    stmt.left = expr;
    return stmt;
}

fn parse_statement(p: *Parser) -> i64 {
    let tok_type: i64 = parser_peek(p);

    if (tok_type == TOK_RETURN) {
        return parse_return_stmt(p);
    }

    if (tok_type == TOK_LET) {
        return parse_let_stmt(p);
    }

    if (tok_type == TOK_LBRACE) {
        return parse_block(p);
    }

    // Default: expression statement
    return parse_expr_stmt(p);
}

fn parse_block(p: *Parser) -> i64 {
    parser_expect(p, TOK_LBRACE);

    let block: *ASTNode = ast_block();
    let max_statements: i64 = 1000;  // Security
    let count: i64 = 0;

    while (parser_peek(p) != TOK_RBRACE && parser_peek(p) != TOK_EOF && count < max_statements) {
        let stmt: *ASTNode = parse_statement(p);
        if (stmt != 0) {
            ast_add_child(block, stmt);
        }
        count = count + 1;
    }

    parser_expect(p, TOK_RBRACE);
    return block;
}

// ==== FUNCTION PARSING ====

fn parse_function(p: *Parser) -> i64 {
    parser_expect(p, TOK_FN);

    let name_tok: *Token = parser_expect(p, TOK_IDENT);
    let func: *ASTNode = ast_function(name_tok.value);

    parser_expect(p, TOK_LPAREN);

    // TODO: Parse parameters

    parser_expect(p, TOK_RPAREN);

    // Optional return type
    if (parser_match(p, TOK_ARROW)) {
        let ret_type_tok: *Token = parser_advance(p);
        func.type_name = ret_type_tok.value;
    }

    // Function body
    let body: *ASTNode = parse_block(p);
    func.body = body;

    return func;
}

// ==== PROGRAM PARSING ====

fn parse_program(p: *Parser) -> i64 {
    let program: *ASTNode = ast_program();
    let max_declarations: i64 = 1000;  // Security
    let count: i64 = 0;

    while (parser_peek(p) != TOK_EOF && count < max_declarations) {
        let tok_type: i64 = parser_peek(p);

        if (tok_type == TOK_FN) {
            let func: *ASTNode = parse_function(p);
            if (func != 0) {
                ast_add_child(program, func);
            }
        } else {
            print("Parse error: unexpected token at top level: ");
            print_int(tok_type);
            println("");
            parser_advance(p);  // Skip invalid token
        }

        count = count + 1;
    }

    return program;
}

// ==== TEST/DEMO MAIN ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS PARSER v1.0");
    println("========================================");
    println("");

    // Test 1: Simple expression
    println("Test 1: Parse expression '2 + 3 * 4'");
    let source1: *i8 = "2 + 3 * 4";
    let tokens1: *TokenList = lex_all(source1);
    let parser1: *Parser = parser_init(tokens1);
    let ast1: *ASTNode = parse_expression(parser1);
    ast_print(ast1);
    print("Eval: ");
    print_int(ast_eval(ast1));
    println("");
    println("");

    // Test 2: Return statement
    println("Test 2: Parse 'return 42;'");
    let source2: *i8 = "return 42;";
    let tokens2: *TokenList = lex_all(source2);
    let parser2: *Parser = parser_init(tokens2);
    let ast2: *ASTNode = parse_return_stmt(parser2);
    ast_print(ast2);
    println("");

    // Test 3: Simple function
    println("Test 3: Parse 'fn test() -> i64 { return 42; }'");
    let source3: *i8 = "fn test() -> i64 { return 42; }";
    let tokens3: *TokenList = lex_all(source3);
    let parser3: *Parser = parser_init(tokens3);
    let ast3: *ASTNode = parse_function(parser3);
    ast_print(ast3);
    println("");

    // Test 4: Nested expression
    println("Test 4: Parse '(10 + 20) * 3'");
    let source4: *i8 = "(10 + 20) * 3";
    let tokens4: *TokenList = lex_all(source4);
    let parser4: *Parser = parser_init(tokens4);
    let ast4: *ASTNode = parse_expression(parser4);
    ast_print(ast4);
    print("Eval: ");
    print_int(ast_eval(ast4));
    println("");
    println("");

    println("✅ Parser tests complete!");

    return 0;
}
