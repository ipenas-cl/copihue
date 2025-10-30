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
