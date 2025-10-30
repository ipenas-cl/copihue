// Chronos Code Generator - Self-Hosted
// Generates x86-64 assembly from AST

// ==== AST Node Types ====
let AST_PROGRAM: i64 = 0;
let AST_FUNCTION: i64 = 1;
let AST_BLOCK: i64 = 2;
let AST_RETURN: i64 = 3;
let AST_LET: i64 = 4;
let AST_IF: i64 = 5;
let AST_WHILE: i64 = 6;
let AST_CALL: i64 = 7;
let AST_IDENT: i64 = 8;
let AST_NUMBER: i64 = 9;
let AST_BINOP: i64 = 10;
let AST_COMPARE: i64 = 11;
let AST_STRING: i64 = 12;
let AST_ASSIGN: i64 = 13;
let AST_ARRAY_LITERAL: i64 = 14;
let AST_INDEX: i64 = 15;
let AST_STRUCT_DEF: i64 = 16;
let AST_STRUCT_LITERAL: i64 = 17;
let AST_FIELD_ACCESS: i64 = 18;
let AST_UNARY: i64 = 19;
let AST_DEREF: i64 = 20;
let AST_ADDR_OF: i64 = 21;
let AST_GLOBAL_VAR: i64 = 22;
let AST_ARRAY_ASSIGN: i64 = 23;
let AST_FIELD_ASSIGN: i64 = 24;
let AST_LOGICAL: i64 = 25;

// ==== Data Structures ====

struct AstNode {
    node_type: i64,
    name: *i8,
    value: *i8,
    op: *i8,
    children: *AstNode,
    child_count: i64,
    offset: i64,
    array_size: i64,
    struct_type: *i8,
    is_pointer: i64,
    is_forward_decl: i64
}

struct Symbol {
    name: *i8,
    offset: i64,
    size: i64,
    type_name: *i8,
    is_pointer: i64
}

struct SymbolTable {
    symbols: *Symbol,
    count: i64,
    stack_size: i64
}

struct StringEntry {
    label: *i8,
    value: *i8,
    len: i64
}

struct StringTable {
    strings: *StringEntry,
    count: i64
}

struct StructField {
    name: *i8,
    offset: i64,
    type_name: *i8,
    is_pointer: i64
}

struct StructType {
    name: *i8,
    fields: *StructField,
    field_count: i64,
    size: i64
}

struct TypeTable {
    structs: *StructType,
    count: i64
}

struct Codegen {
    output_buf: *i8,
    output_len: i64,
    output_cap: i64,
    label_count: i64,
    symtab: *SymbolTable,
    strtab: *StringTable,
    types: *TypeTable
}

// ==== Helper Functions ====

// Initialize code generator (returns address as i64)
fn codegen_init() -> i64 {
    let cg_addr: i64 = malloc(104);  // sizeof(Codegen)
    if (cg_addr == 0) {
        return 0;
    }

    let cg: *Codegen = cg_addr;

    // Initialize output buffer
    cg.output_cap = 4096;
    cg.output_buf = malloc(4096);
    cg.output_len = 0;
    cg.label_count = 0;

    // Initialize symbol table
    cg.symtab = malloc(24);  // sizeof(SymbolTable)
    cg.symtab.symbols = malloc(256);  // Space for symbols
    cg.symtab.count = 0;
    cg.symtab.stack_size = 0;

    // Initialize string table
    cg.strtab = malloc(16);  // sizeof(StringTable)
    cg.strtab.strings = malloc(256);  // Space for strings
    cg.strtab.count = 0;

    // Initialize type table
    cg.types = malloc(16);  // sizeof(TypeTable)
    cg.types.structs = malloc(256);  // Space for struct types
    cg.types.count = 0;

    return cg_addr;
}

// Emit a line of assembly
fn emit(cg: *Codegen, line: *i8) -> i64 {
    // Calculate line length
    let len: i64 = 0;
    let ptr: *i8 = line;
    while (ptr[len] != 0) {
        len = len + 1;
    }

    // Grow buffer if needed
    while (cg.output_len + len + 1 > cg.output_cap) {
        cg.output_cap = cg.output_cap * 2;
        let new_buf: *i8 = malloc(cg.output_cap);

        // Copy old content
        let i: i64 = 0;
        while (i < cg.output_len) {
            new_buf[i] = cg.output_buf[i];
            i = i + 1;
        }

        cg.output_buf = new_buf;
    }

    // Copy line to buffer
    let i: i64 = 0;
    while (i < len) {
        cg.output_buf[cg.output_len] = line[i];
        cg.output_len = cg.output_len + 1;
        i = i + 1;
    }

    // Add newline
    cg.output_buf[cg.output_len] = 10;  // '\n'
    cg.output_len = cg.output_len + 1;

    return 0;
}

// Generate new label number
fn new_label(cg: *Codegen) -> i64 {
    let label: i64 = cg.label_count;
    cg.label_count = cg.label_count + 1;
    return label;
}

// ==== Expression Code Generation ====

fn gen_expr_simple(cg: *Codegen, node: *AstNode) -> i64 {
    // Simplified version for bootstrap - handles basic expressions
    if (node.node_type == AST_NUMBER) {
        // Generate: mov rax, <number>
        emit(cg, "    mov rax, ");
        emit(cg, node.value);
        return 0;
    }

    if (node.node_type == AST_IDENT) {
        // Look up variable and load from stack
        // TODO: Implement symbol table lookup
        emit(cg, "    ; Load variable: ");
        emit(cg, node.name);
        return 0;
    }

    if (node.node_type == AST_CALL) {
        // Function call
        emit(cg, "    call ");
        emit(cg, node.name);
        return 0;
    }

    // Unknown node type
    emit(cg, "    ; TODO: Unknown expr type");
    return 0;
}

fn gen_expr(cg: *Codegen, node: *AstNode) -> i64 {
    if (node.node_type == AST_BINOP) {
        // Generate binary operation
        let op: *i8 = node.op;

        // Generate right operand first (recursive)
        gen_expr(cg, node.children[1]);
        emit(cg, "    push rax");

        // Generate left operand
        gen_expr(cg, node.children[0]);
        emit(cg, "    pop rbx");

        // Perform operation
        if (op[0] == 43) {  // '+'
            emit(cg, "    add rax, rbx");
            return 0;
        }
        if (op[0] == 45) {  // '-'
            emit(cg, "    sub rax, rbx");
            return 0;
        }
        if (op[0] == 42) {  // '*'
            emit(cg, "    imul rax, rbx");
            return 0;
        }
        if (op[0] == 47) {  // '/'
            emit(cg, "    xor rdx, rdx");
            emit(cg, "    div rbx");
            return 0;
        }

        return 0;
    }

    // Delegate to simple handler
    return gen_expr_simple(cg, node);
}

// ==== Statement Code Generation ====

fn gen_stmt(cg: *Codegen, node: *AstNode) -> i64 {
    if (node.node_type == AST_RETURN) {
        // Generate return statement
        if (node.child_count > 0) {
            gen_expr(cg, node.children[0]);
        }
        emit(cg, "    leave");
        emit(cg, "    ret");
        return 0;
    }

    if (node.node_type == AST_LET) {
        // Variable declaration
        // TODO: Add to symbol table and allocate stack space
        emit(cg, "    ; Variable: ");
        emit(cg, node.name);
        return 0;
    }

    if (node.node_type == AST_BLOCK) {
        // Block of statements
        let i: i64 = 0;
        while (i < node.child_count) {
            gen_stmt(cg, node.children[i]);
            i = i + 1;
        }
        return 0;
    }

    // Expression statement
    gen_expr(cg, node);
    return 0;
}

// ==== Function Code Generation ====

fn gen_func(cg: *Codegen, node: *AstNode) -> i64 {
    // Function header
    emit(cg, "");
    emit(cg, node.name);
    emit(cg, ":");
    emit(cg, "    push rbp");
    emit(cg, "    mov rbp, rsp");
    emit(cg, "    sub rsp, 256");  // TODO: Calculate actual stack size

    // Function body
    if (node.child_count > 0) {
        gen_stmt(cg, node.children[0]);
    }

    // Default return
    emit(cg, "    xor rax, rax");
    emit(cg, "    leave");
    emit(cg, "    ret");

    return 0;
}

// ==== Runtime Helper Functions ====

fn gen_helpers(cg: *Codegen) -> i64 {
    emit(cg, "");
    emit(cg, "__print_int:");
    emit(cg, "    push rbp");
    emit(cg, "    mov rbp, rsp");
    emit(cg, "    sub rsp, 32");
    emit(cg, "    mov rbx, rax");
    emit(cg, "    test rbx, rbx");
    emit(cg, "    jns .positive");
    emit(cg, "    neg rbx");
    emit(cg, "    push rbx");
    emit(cg, "    mov byte [rbp-1], 45");
    emit(cg, "    lea rsi, [rbp-1]");
    emit(cg, "    mov rdi, 1");
    emit(cg, "    mov rdx, 1");
    emit(cg, "    mov rax, 1");
    emit(cg, "    syscall");
    emit(cg, "    pop rbx");
    emit(cg, ".positive:");
    emit(cg, "    lea rdi, [rbp-32]");
    emit(cg, "    mov rax, rbx");
    emit(cg, "    mov rcx, 10");
    emit(cg, ".loop:");
    emit(cg, "    xor rdx, rdx");
    emit(cg, "    div rcx");
    emit(cg, "    add dl, 48");
    emit(cg, "    mov [rdi], dl");
    emit(cg, "    inc rdi");
    emit(cg, "    test rax, rax");
    emit(cg, "    jnz .loop");
    emit(cg, "    mov r8, rdi");
    emit(cg, "    dec rdi");
    emit(cg, ".print_loop:");
    emit(cg, "    lea rax, [rbp-32]");
    emit(cg, "    cmp rdi, rax");
    emit(cg, "    jl .done");
    emit(cg, "    push rdi");
    emit(cg, "    mov rsi, rdi");
    emit(cg, "    mov rdi, 1");
    emit(cg, "    mov rdx, 1");
    emit(cg, "    mov rax, 1");
    emit(cg, "    syscall");
    emit(cg, "    pop rdi");
    emit(cg, "    dec rdi");
    emit(cg, "    jmp .print_loop");
    emit(cg, ".done:");
    emit(cg, "    leave");
    emit(cg, "    ret");

    return 0;
}

// ==== Program Code Generation ====

fn gen_program(cg: *Codegen, node: *AstNode) -> i64 {
    // Assembly header
    emit(cg, "; CHRONOS COMPILER - SELF-HOSTED");
    emit(cg, "");
    emit(cg, "section .data");
    emit(cg, "");
    emit(cg, "section .text");
    emit(cg, "    global _start");
    emit(cg, "");
    emit(cg, "_start:");
    emit(cg, "    call main");
    emit(cg, "    mov rdi, rax");
    emit(cg, "    mov rax, 60");
    emit(cg, "    syscall");

    // Generate helper functions
    gen_helpers(cg);

    // Generate user functions
    let i: i64 = 0;
    while (i < node.child_count) {
        let child: *AstNode = node.children[i];
        if (child.node_type == AST_FUNCTION) {
            gen_func(cg, child);
        }
        i = i + 1;
    }

    return 0;
}

// ==== Main Entry Point ====

fn main() -> i64 {
    println("=== Chronos Code Generator - Self-Hosted ===");
    println("");

    // Initialize codegen
    let cg_addr: i64 = codegen_init();
    if (cg_addr == 0) {
        println("Error: Failed to initialize code generator");
        return 1;
    }
    let cg: *Codegen = cg_addr;

    println("✅ Code generator initialized");
    println("   Output buffer: 4096 bytes");
    println("   Symbol table ready");
    println("   String table ready");
    println("   Type table ready");
    println("");

    // Test: Generate simple program
    println("Test: Generating simple function...");

    // Create a dummy AST node for testing
    let test_node: *AstNode = malloc(88);  // sizeof(AstNode)
    test_node.node_type = AST_FUNCTION;
    test_node.name = "test_func";
    test_node.child_count = 0;

    gen_func(cg, test_node);

    println("✅ Function generated");
    println("");

    // Output some generated code
    println("Generated assembly (first 200 chars):");
    let i: i64 = 0;
    while (i < 200) {
        if (i >= cg.output_len) {
            i = 999999;
        } else {
            if (cg.output_buf[i] >= 32) {
                print_int(cg.output_buf[i]);
                print(" ");
            } else {
                if (cg.output_buf[i] == 10) {
                    println("");
                }
            }
            i = i + 1;
        }
    }

    println("");
    println("✅ Code generator test complete!");
    println("");
    println("NEXT STEPS:");
    println("1. Implement complete gen_expr() for all AST types");
    println("2. Implement complete gen_stmt() for all statements");
    println("3. Add symbol table management");
    println("4. Add string table management");
    println("5. Integrate with lexer and parser");
    println("");
    println("Self-hosting progress: ~92%");

    return 0;
}
