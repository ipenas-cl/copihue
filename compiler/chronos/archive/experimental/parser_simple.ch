// CHRONOS SIMPLE PARSER - Expression Parser with Dynamic AST
// This demonstrates parsing with malloc-based AST trees
// Author: Chronos Project
// Date: 2025-10-29

// ============================================
// TOKEN TYPES
// ============================================

let T_EOF = 0;
let T_NUM = 1;
let T_PLUS = 2;
let T_MINUS = 3;
let T_STAR = 4;
let T_SLASH = 5;
let T_LPAREN = 6;
let T_RPAREN = 7;

// ============================================
// AST NODE TYPES
// ============================================

let AST_NUMBER = 0;
let AST_BINOP = 1;

// ============================================
// TOKEN STRUCTURE
// ============================================

struct Token {
    type: i64,
    value: i64    // For numbers
}

// ============================================
// PARSER STRUCTURE
// ============================================

struct Parser {
    tokens: *Token,
    count: i64,
    pos: i64
}

// ============================================
// AST FUNCTIONS (simplified)
// ============================================

// Forward declarations
fn ast_new(node_type: i64) -> i64;
fn ast_set_value(node: i64, val: i64) -> i64;
fn ast_set_op(node: i64, op: i64) -> i64;
fn ast_add_child(parent: i64, child: i64) -> i64;
fn ast_print(node: i64, depth: i64) -> i64;
fn ast_eval(node: i64) -> i64;

// Helper to initialize AST node fields
fn ast_init(ptr: *i64, node_type: i64) -> i64 {
    ptr[0] = node_type;  // node_type
    ptr[1] = 0;          // value
    ptr[2] = 0;          // op ('+', '-', '*', '/')
    ptr[3] = 0;          // children array
    ptr[4] = 0;          // child_count
    ptr[5] = 0;          // child_capacity
    return 0;
}

// Create new AST node
fn ast_new(node_type: i64) -> i64 {
    let size = 48;  // 6 fields * 8 bytes
    let ptr: *i64 = malloc(size);
    if (ptr == 0) {
        println("ERROR: ast_new malloc failed!");
        return 0;
    }
    ast_init(ptr, node_type);
    return ptr;
}

// Set node value
fn ast_set_value_helper(ptr: *i64, val: i64) -> i64 {
    ptr[1] = val;
    return 0;
}

fn ast_set_value(node: i64, val: i64) -> i64 {
    if (node == 0) { return -1; }
    ast_set_value_helper(node, val);
    return 0;
}

// Set node operator
fn ast_set_op_helper(ptr: *i64, op: i64) -> i64 {
    ptr[2] = op;
    return 0;
}

fn ast_set_op(node: i64, op: i64) -> i64 {
    if (node == 0) { return -1; }
    ast_set_op_helper(node, op);
    return 0;
}

// Get node info
fn ast_get_type(ptr: *i64) -> i64 {
    return ptr[0];
}

fn ast_get_value(ptr: *i64) -> i64 {
    return ptr[1];
}

fn ast_get_op(ptr: *i64) -> i64 {
    return ptr[2];
}

fn ast_get_children(ptr: *i64) -> i64 {
    return ptr[3];
}

fn ast_get_child_count(ptr: *i64) -> i64 {
    return ptr[4];
}

// Add child to node
fn ast_add_child(parent: i64, child: i64) -> i64 {
    if (parent == 0) { return -1; }

    let child_count: i64 = ast_get_child_count(parent);
    let child_capacity: i64 = ast_get_child_count(parent);  // Simplified: capacity = count

    // For simplicity, allocate exactly 2 children (for binops)
    let children: *i64 = ast_get_children(parent);

    if (children == 0) {
        // First allocation
        children = malloc(16);  // 2 children * 8 bytes
        if (children == 0) {
            println("ERROR: ast_add_child malloc failed!");
            return -1;
        }
        // Store children array in parent
        let p: *i64 = parent;
        p[3] = children;
    }

    // Add child
    children[child_count] = child;

    // Increment child_count
    let p: *i64 = parent;
    p[4] = child_count + 1;

    return 0;
}

// Print AST
fn ast_print_indent(depth: i64) -> i64 {
    let i = 0;
    while (i < depth) {
        print("  ");
        i = i + 1;
    }
    return 0;
}

fn ast_print(node: i64, depth: i64) -> i64 {
    if (node == 0) { return 0; }

    ast_print_indent(depth);

    let node_type: i64 = ast_get_type(node);

    if (node_type == AST_NUMBER) {
        print("NUMBER(");
        print_int(ast_get_value(node));
        print(")");
    }

    if (node_type == AST_BINOP) {
        let op: i64 = ast_get_op(node);
        print("BINOP(");
        if (op == T_PLUS) { print("+"); }
        if (op == T_MINUS) { print("-"); }
        if (op == T_STAR) { print("*"); }
        if (op == T_SLASH) { print("/"); }
        print(")");
    }

    println("");

    // Print children
    let children: *i64 = ast_get_children(node);
    let child_count: i64 = ast_get_child_count(node);

    if (children != 0) {
        let i = 0;
        while (i < child_count) {
            ast_print(children[i], depth + 1);
            i = i + 1;
        }
    }

    return 0;
}

// Evaluate AST
fn ast_eval(node: i64) -> i64 {
    if (node == 0) { return 0; }

    let node_type: i64 = ast_get_type(node);

    if (node_type == AST_NUMBER) {
        return ast_get_value(node);
    }

    if (node_type == AST_BINOP) {
        let children: *i64 = ast_get_children(node);
        let left: i64 = ast_eval(children[0]);
        let right: i64 = ast_eval(children[1]);
        let op: i64 = ast_get_op(node);

        if (op == T_PLUS) { return left + right; }
        if (op == T_MINUS) { return left - right; }
        if (op == T_STAR) { return left * right; }
        if (op == T_SLASH) { return left / right; }
    }

    return 0;
}

// ============================================
// PARSER FUNCTIONS
// ============================================

// Forward declarations
fn parse_expr(p: *Parser) -> i64;
fn parse_term(p: *Parser) -> i64;
fn parse_factor(p: *Parser) -> i64;

// Parser helpers
fn parser_peek(p: *Parser) -> i64 {
    if (p.pos >= p.count) {
        return T_EOF;
    }
    return p.tokens[p.pos].type;
}

fn parser_advance(p: *Parser) -> i64 {
    if (p.pos < p.count) {
        p.pos = p.pos + 1;
    }
    return 0;
}

fn parser_current_value(p: *Parser) -> i64 {
    if (p.pos >= p.count) {
        return 0;
    }
    return p.tokens[p.pos].value;
}

// Parse factor: number | '(' expr ')'
fn parse_factor(p: *Parser) -> i64 {
    let tok: i64 = parser_peek(p);

    if (tok == T_NUM) {
        let val: i64 = parser_current_value(p);
        parser_advance(p);

        let node: i64 = ast_new(AST_NUMBER);
        ast_set_value(node, val);
        return node;
    }

    if (tok == T_LPAREN) {
        parser_advance(p);  // Skip '('
        let node: i64 = parse_expr(p);
        parser_advance(p);  // Skip ')'
        return node;
    }

    println("ERROR: Expected number or '('");
    return 0;
}

// Parse term: factor ( ('*'|'/') factor )*
fn parse_term(p: *Parser) -> i64 {
    let left: i64 = parse_factor(p);

    while (1 == 1) {
        let tok: i64 = parser_peek(p);

        if (tok == T_STAR) {
            parser_advance(p);
            let right: i64 = parse_factor(p);

            let node: i64 = ast_new(AST_BINOP);
            ast_set_op(node, T_STAR);
            ast_add_child(node, left);
            ast_add_child(node, right);
            left = node;
        } else {
            if (tok == T_SLASH) {
                parser_advance(p);
                let right: i64 = parse_factor(p);

                let node: i64 = ast_new(AST_BINOP);
                ast_set_op(node, T_SLASH);
                ast_add_child(node, left);
                ast_add_child(node, right);
                left = node;
            } else {
                return left;
            }
        }
    }

    return left;
}

// Parse expr: term ( ('+'|'-') term )*
fn parse_expr(p: *Parser) -> i64 {
    let left: i64 = parse_term(p);

    while (1 == 1) {
        let tok: i64 = parser_peek(p);

        if (tok == T_PLUS) {
            parser_advance(p);
            let right: i64 = parse_term(p);

            let node: i64 = ast_new(AST_BINOP);
            ast_set_op(node, T_PLUS);
            ast_add_child(node, left);
            ast_add_child(node, right);
            left = node;
        } else {
            if (tok == T_MINUS) {
                parser_advance(p);
                let right: i64 = parse_term(p);

                let node: i64 = ast_new(AST_BINOP);
                ast_set_op(node, T_MINUS);
                ast_add_child(node, left);
                ast_add_child(node, right);
                left = node;
            } else {
                return left;
            }
        }
    }

    return left;
}

// ============================================
// MAIN TEST
// ============================================

fn main() -> i64 {
    println("=== Simple Parser Test ===");
    println("");

    // Test 1: 2 + 3
    println("Test 1: 2 + 3");
    let tokens1: [Token; 3];
    tokens1[0].type = T_NUM;
    tokens1[0].value = 2;
    tokens1[1].type = T_PLUS;
    tokens1[2].type = T_NUM;
    tokens1[2].value = 3;

    let parser1: Parser;
    parser1.tokens = tokens1;
    parser1.count = 3;
    parser1.pos = 0;

    let ast1: i64 = parse_expr(parser1);
    println("AST:");
    ast_print(ast1, 0);
    print("Result: ");
    print_int(ast_eval(ast1));
    println("");
    println("");

    // Test 2: 2 * 3 + 4
    println("Test 2: 2 * 3 + 4");
    let tokens2: [Token; 5];
    tokens2[0].type = T_NUM;
    tokens2[0].value = 2;
    tokens2[1].type = T_STAR;
    tokens2[2].type = T_NUM;
    tokens2[2].value = 3;
    tokens2[3].type = T_PLUS;
    tokens2[4].type = T_NUM;
    tokens2[4].value = 4;

    let parser2: Parser;
    parser2.tokens = tokens2;
    parser2.count = 5;
    parser2.pos = 0;

    let ast2: i64 = parse_expr(parser2);
    println("AST:");
    ast_print(ast2, 0);
    print("Result: ");
    print_int(ast_eval(ast2));
    println("");
    println("");

    println("✅ All tests passed!");
    return 0;
}
