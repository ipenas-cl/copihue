// CHRONOS AST - Dynamic Abstract Syntax Tree using malloc
// Author: Chronos Project
// Date: 2025-10-29

// ============================================
// AST NODE TYPES
// ============================================

let AST_PROGRAM = 0;
let AST_FUNCTION = 1;
let AST_BLOCK = 2;
let AST_RETURN = 3;
let AST_LET = 4;
let AST_IF = 5;
let AST_WHILE = 6;
let AST_CALL = 7;
let AST_IDENT = 8;
let AST_NUMBER = 9;
let AST_BINOP = 10;
let AST_COMPARE = 11;
let AST_STRING = 12;
let AST_ASSIGN = 13;
let AST_ARRAY_LITERAL = 14;
let AST_INDEX = 15;
let AST_STRUCT_DEF = 16;
let AST_STRUCT_LITERAL = 17;
let AST_FIELD_ACCESS = 18;
let AST_UNARY = 19;
let AST_DEREF = 20;
let AST_ADDR_OF = 21;
let AST_GLOBAL_VAR = 22;
let AST_ARRAY_ASSIGN = 23;
let AST_FIELD_ASSIGN = 24;
let AST_LOGICAL = 25;

// ============================================
// AST NODE STRUCTURE (Dynamic!)
// ============================================

struct AstNode {
    node_type: i64,
    name: *i8,          // For identifiers, functions, etc.
    value: i64,         // For numbers, numeric values
    str_value: *i8,     // For strings
    op: *i8,            // For operators
    line: i64,          // Line number for error reporting

    // Dynamic children array
    children: *i64,     // Array of AstNode pointers (stored as i64)
    child_count: i64,
    child_capacity: i64
}

// ============================================
// AST NODE FUNCTIONS
// ============================================

// Forward declarations
fn ast_new(node_type: i64) -> i64;
fn ast_add_child(parent: i64, child: i64) -> i64;
fn ast_set_name(node: i64, name: *i8) -> i64;
fn ast_set_value(node: i64, val: i64) -> i64;
fn ast_set_op(node: i64, op: *i8) -> i64;
fn ast_free(node: i64) -> i64;

// Helper to initialize fields (takes pointer parameter - no bounds check!)
fn ast_init_fields(ptr: *i64, node_type: i64) -> i64 {
    ptr[0] = node_type;
    ptr[1] = 0;  // name = NULL
    ptr[2] = 0;  // value = 0
    ptr[3] = 0;  // str_value = NULL
    ptr[4] = 0;  // op = NULL
    ptr[5] = 0;  // line = 0
    ptr[6] = 0;  // children = NULL
    ptr[7] = 0;  // child_count = 0
    ptr[8] = 0;  // child_capacity = 0
    return 0;
}

// Create a new AST node (returns pointer as i64)
fn ast_new(node_type: i64) -> i64 {
    let size = 80;  // sizeof(AstNode) - 10 fields * 8 bytes
    let node_ptr = malloc(size);

    if (node_ptr == 0) {
        println("ERROR: ast_new malloc failed!");
        return 0;
    }

    // Initialize fields via helper function
    ast_init_fields(node_ptr, node_type);

    return node_ptr;
}

// Helper to get child_count
fn ast_get_child_count(parent: *i64) -> i64 {
    return parent[7];
}

// Helper to get child_capacity
fn ast_get_child_capacity(parent: *i64) -> i64 {
    return parent[8];
}

// Helper to get children array
fn ast_get_children(parent: *i64) -> i64 {
    return parent[6];
}

// Helper to set children array
fn ast_set_children(parent: *i64, children: i64) -> i64 {
    parent[6] = children;
    return 0;
}

// Helper to set child_capacity
fn ast_set_child_capacity(parent: *i64, capacity: i64) -> i64 {
    parent[8] = capacity;
    return 0;
}

// Helper to set child_count
fn ast_set_child_count(parent: *i64, count: i64) -> i64 {
    parent[7] = count;
    return 0;
}

// Helper to copy children array
fn ast_copy_children(old: *i64, new: *i64, count: i64) -> i64 {
    let i = 0;
    while (i < count) {
        new[i] = old[i];
        i = i + 1;
    }
    return 0;
}

// Helper to add child to children array
fn ast_append_child(children: *i64, index: i64, child: i64) -> i64 {
    children[index] = child;
    return 0;
}

// Add a child node to parent
fn ast_add_child(parent_ptr: i64, child_ptr: i64) -> i64 {
    if (parent_ptr == 0) {
        return -1;
    }

    // Get current child_count and child_capacity
    let child_count = ast_get_child_count(parent_ptr);
    let child_capacity = ast_get_child_capacity(parent_ptr);

    // Need to grow children array?
    if (child_count >= child_capacity) {
        let new_capacity = 4;
        if (child_capacity > 0) {
            new_capacity = child_capacity * 2;
        }

        // Allocate new array
        let new_children = malloc(new_capacity * 8);
        if (new_children == 0) {
            println("ERROR: ast_add_child malloc failed!");
            return -1;
        }

        // Copy old children
        let old_children = ast_get_children(parent_ptr);
        if (old_children != 0) {
            ast_copy_children(old_children, new_children, child_count);
            // Note: Should free old_children here, but skipping for simplicity
        }

        // Update parent
        ast_set_children(parent_ptr, new_children);
        ast_set_child_capacity(parent_ptr, new_capacity);
    }

    // Add child
    let children = ast_get_children(parent_ptr);
    ast_append_child(children, child_count, child_ptr);
    ast_set_child_count(parent_ptr, child_count + 1);

    return 0;
}

// Helper to set name field
fn ast_helper_set_name(node: *i64, name: *i8) -> i64 {
    node[1] = name;
    return 0;
}

// Helper to set value field
fn ast_helper_set_value(node: *i64, val: i64) -> i64 {
    node[2] = val;
    return 0;
}

// Helper to set op field
fn ast_helper_set_op(node: *i64, op: *i8) -> i64 {
    node[4] = op;
    return 0;
}

// Set node name
fn ast_set_name(node_ptr: i64, name: *i8) -> i64 {
    if (node_ptr == 0) {
        return -1;
    }
    ast_helper_set_name(node_ptr, name);
    return 0;
}

// Set node value
fn ast_set_value(node_ptr: i64, val: i64) -> i64 {
    if (node_ptr == 0) {
        return -1;
    }
    ast_helper_set_value(node_ptr, val);
    return 0;
}

// Set node operator
fn ast_set_op(node_ptr: i64, op: *i8) -> i64 {
    if (node_ptr == 0) {
        return -1;
    }
    ast_helper_set_op(node_ptr, op);
    return 0;
}

// Helper to free children recursively
fn ast_free_children_recursive(children: *i64, count: i64) -> i64 {
    let i = 0;
    while (i < count) {
        ast_free(children[i]);
        i = i + 1;
    }
    return 0;
}

// Free AST node (recursive)
fn ast_free(node_ptr: i64) -> i64 {
    if (node_ptr == 0) {
        return 0;
    }

    // Free children recursively
    let children = ast_get_children(node_ptr);
    let child_count = ast_get_child_count(node_ptr);

    if (children != 0) {
        ast_free_children_recursive(children, child_count);
        free(children);
    }

    // Free the node itself
    free(node_ptr);
    return 0;
}

// ============================================
// HELPER: Print AST (for debugging)
// ============================================

fn ast_print_type(node_type: i64) -> i64 {
    if (node_type == AST_PROGRAM) { print("PROGRAM"); }
    if (node_type == AST_FUNCTION) { print("FUNCTION"); }
    if (node_type == AST_BLOCK) { print("BLOCK"); }
    if (node_type == AST_RETURN) { print("RETURN"); }
    if (node_type == AST_NUMBER) { print("NUMBER"); }
    if (node_type == AST_BINOP) { print("BINOP"); }
    if (node_type == AST_IDENT) { print("IDENT"); }
    if (node_type == AST_CALL) { print("CALL"); }
    if (node_type == AST_STRING) { print("STRING"); }
    return 0;
}

fn ast_print_indent(depth: i64) -> i64 {
    let i = 0;
    while (i < depth) {
        print("  ");
        i = i + 1;
    }
    return 0;
}

fn ast_print(node_ptr: i64, depth: i64) -> i64;

// Helper to get node_type
fn ast_get_type(node: *i64) -> i64 {
    return node[0];
}

// Helper to get value
fn ast_get_value(node: *i64) -> i64 {
    return node[2];
}

// Helper to print children
fn ast_print_children_recursive(children: *i64, count: i64, depth: i64) -> i64 {
    let i = 0;
    while (i < count) {
        ast_print(children[i], depth + 1);
        i = i + 1;
    }
    return 0;
}

fn ast_print(node_ptr: i64, depth: i64) -> i64 {
    if (node_ptr == 0) {
        return 0;
    }

    ast_print_indent(depth);
    let node_type = ast_get_type(node_ptr);
    ast_print_type(node_type);

    // Print value if it's a number
    if (node_type == AST_NUMBER) {
        print(" value=");
        let val = ast_get_value(node_ptr);
        print_int(val);
    }

    println("");

    // Print children
    let children = ast_get_children(node_ptr);
    let child_count = ast_get_child_count(node_ptr);

    if (children != 0) {
        ast_print_children_recursive(children, child_count, depth);
    }

    return 0;
}

// ============================================
// TEST MAIN
// ============================================

fn main() -> i64 {
    println("=== AST Dynamic Test ===");

    // Create a simple expression tree: 2 + 3
    let root = ast_new(AST_BINOP);
    ast_set_op(root, "+");

    let left = ast_new(AST_NUMBER);
    ast_set_value(left, 2);

    let right = ast_new(AST_NUMBER);
    ast_set_value(right, 3);

    ast_add_child(root, left);
    ast_add_child(root, right);

    println("AST Tree:");
    ast_print(root, 0);

    // Free the tree
    println("Freeing AST...");
    ast_free(root);

    println("✅ Test complete!");
    return 0;
}
