# Diseño del AST (Abstract Syntax Tree) - Chronos Compiler

**Versión:** 1.0
**Fecha:** 29 de octubre de 2025
**Para:** Chronos Self-Hosted Compiler v1.0

---

## 1. Objetivo

Crear una representación **recursiva en árbol** del código fuente que:
- Capture la estructura jerárquica del programa
- Soporte expresiones anidadas
- Facilite análisis semántico y generación de código

### Problema Actual

```chronos
// ANTES: Estructura plana (no puede representar (a+b)*(c+d))
struct Expr {
    op: i64,
    left: i64,      // Solo números
    right: i64      // Solo números
}
```

❌ No puede representar `(10 + 20) * (30 + 40)`

### Solución Propuesta

```chronos
// DESPUÉS: Árbol recursivo
struct ASTNode {
    node_type: i64,
    left: *ASTNode,   // Puntero a hijo izquierdo (recursivo!)
    right: *ASTNode,  // Puntero a hijo derecho (recursivo!)
    value: i64,
    name: *i8
}
```

✅ Puede representar cualquier expresión anidada!

---

## 2. Tipos de Nodos (NodeType)

```chronos
// Node types (usando constantes i64)

// ===== EXPRESSIONS =====
let NODE_NUMBER: i64 = 1;           // Literal number: 42
let NODE_IDENT: i64 = 2;            // Variable reference: x
let NODE_BINARY_OP: i64 = 3;        // Binary operation: a + b
let NODE_UNARY_OP: i64 = 4;         // Unary operation: -x
let NODE_CALL: i64 = 5;             // Function call: foo(a, b)
let NODE_FIELD_ACCESS: i64 = 6;     // Field access: obj.field

// ===== STATEMENTS =====
let NODE_LET: i64 = 10;             // Variable declaration: let x: i64;
let NODE_ASSIGN: i64 = 11;          // Assignment: x = 42;
let NODE_RETURN: i64 = 12;          // Return: return 42;
let NODE_IF: i64 = 13;              // If statement
let NODE_WHILE: i64 = 14;           // While loop
let NODE_BLOCK: i64 = 15;           // Block: { ... }

// ===== DECLARATIONS =====
let NODE_FUNCTION: i64 = 20;        // Function declaration
let NODE_PARAM: i64 = 21;           // Function parameter
let NODE_STRUCT: i64 = 22;          // Struct declaration
let NODE_FIELD_DECL: i64 = 23;      // Struct field declaration

// ===== TYPES =====
let NODE_TYPE_I64: i64 = 30;        // Type: i64
let NODE_TYPE_I8: i64 = 31;         // Type: i8
let NODE_TYPE_PTR: i64 = 32;        // Type: *T
let NODE_TYPE_ARRAY: i64 = 33;      // Type: [T; N]
let NODE_TYPE_CUSTOM: i64 = 34;     // Type: CustomStruct

// ===== PROGRAM =====
let NODE_PROGRAM: i64 = 40;         // Root node
```

---

## 3. Estructura del ASTNode

```chronos
struct ASTNode {
    // Node type (one of NODE_* constants)
    node_type: i64,

    // For binary operations (+, -, *, /, ==, etc.)
    op: i64,              // Operator type (TOK_PLUS, TOK_MINUS, etc.)

    // Children (recursive pointers!)
    left: *ASTNode,       // Left child
    right: *ASTNode,      // Right child
    body: *ASTNode,       // Function body, block body, etc.
    condition: *ASTNode,  // If/while condition

    // Values
    value: i64,           // For NODE_NUMBER: the number value
    name: *i8,            // For NODE_IDENT, NODE_FUNCTION: name

    // Type information
    type_node: *ASTNode,  // Type annotation (for variables, functions)

    // Lists (for function params, struct fields, etc.)
    children: *ASTNode,   // Linked list of child nodes
    next: *ASTNode,       // Next node in linked list

    // Source location (for error messages)
    line: i64,
    column: i64
}
```

**Tamaño:** ~80 bytes (10 fields × 8 bytes)

---

## 4. Representación de Estructuras del Lenguaje

### 4.1 Expresiones

#### Número Literal: `42`
```
NODE_NUMBER
├─ value: 42
├─ left: NULL
└─ right: NULL
```

#### Variable: `x`
```
NODE_IDENT
├─ name: "x"
├─ left: NULL
└─ right: NULL
```

#### Suma Simple: `10 + 20`
```
NODE_BINARY_OP (op=TOK_PLUS)
├─ left: NODE_NUMBER (value=10)
└─ right: NODE_NUMBER (value=20)
```

#### Expresión Anidada: `(10 + 20) * (30 + 40)`
```
NODE_BINARY_OP (op=TOK_STAR)
├─ left: NODE_BINARY_OP (op=TOK_PLUS)
│  ├─ left: NODE_NUMBER (value=10)
│  └─ right: NODE_NUMBER (value=20)
└─ right: NODE_BINARY_OP (op=TOK_PLUS)
   ├─ left: NODE_NUMBER (value=30)
   └─ right: NODE_NUMBER (value=40)
```

✅ ¡Ahora podemos representar cualquier expresión anidada!

#### Field Access: `p.x`
```
NODE_FIELD_ACCESS
├─ left: NODE_IDENT (name="p")
└─ name: "x"
```

#### Field Access en Expresión: `p.x + p.y`
```
NODE_BINARY_OP (op=TOK_PLUS)
├─ left: NODE_FIELD_ACCESS
│  ├─ left: NODE_IDENT (name="p")
│  └─ name: "x"
└─ right: NODE_FIELD_ACCESS
   ├─ left: NODE_IDENT (name="p")
   └─ name: "y"
```

### 4.2 Statements

#### Variable Declaration: `let x: i64;`
```
NODE_LET
├─ name: "x"
├─ type_node: NODE_TYPE_I64
└─ right: NULL (no initializer)
```

#### Variable Declaration con Initializer: `let x: i64 = 42;`
```
NODE_LET
├─ name: "x"
├─ type_node: NODE_TYPE_I64
└─ right: NODE_NUMBER (value=42)
```

#### Assignment: `x = 10 + 20;`
```
NODE_ASSIGN
├─ left: NODE_IDENT (name="x")
└─ right: NODE_BINARY_OP (op=TOK_PLUS)
   ├─ left: NODE_NUMBER (value=10)
   └─ right: NODE_NUMBER (value=20)
```

#### Return Statement: `return 42;`
```
NODE_RETURN
├─ left: NODE_NUMBER (value=42)
└─ right: NULL
```

#### Return Expression: `return x + y;`
```
NODE_RETURN
├─ left: NODE_BINARY_OP (op=TOK_PLUS)
│  ├─ left: NODE_IDENT (name="x")
│  └─ right: NODE_IDENT (name="y")
└─ right: NULL
```

#### If Statement: `if (x == 10) { return 42; }`
```
NODE_IF
├─ condition: NODE_BINARY_OP (op=TOK_EQEQ)
│  ├─ left: NODE_IDENT (name="x")
│  └─ right: NODE_NUMBER (value=10)
├─ body: NODE_BLOCK
│  └─ children: NODE_RETURN
│     └─ left: NODE_NUMBER (value=42)
└─ right: NULL (no else)
```

#### While Loop: `while (i < 10) { i = i + 1; }`
```
NODE_WHILE
├─ condition: NODE_BINARY_OP (op=TOK_LT)
│  ├─ left: NODE_IDENT (name="i")
│  └─ right: NODE_NUMBER (value=10)
└─ body: NODE_BLOCK
   └─ children: NODE_ASSIGN
      ├─ left: NODE_IDENT (name="i")
      └─ right: NODE_BINARY_OP (op=TOK_PLUS)
         ├─ left: NODE_IDENT (name="i")
         └─ right: NODE_NUMBER (value=1)
```

### 4.3 Declarations

#### Function: `fn add(a: i64, b: i64) -> i64 { return a + b; }`
```
NODE_FUNCTION
├─ name: "add"
├─ children: [Linked list of params]
│  ├─ NODE_PARAM (name="a", type=NODE_TYPE_I64) → next →
│  └─ NODE_PARAM (name="b", type=NODE_TYPE_I64) → next → NULL
├─ type_node: NODE_TYPE_I64 (return type)
└─ body: NODE_BLOCK
   └─ children: NODE_RETURN
      └─ left: NODE_BINARY_OP (op=TOK_PLUS)
         ├─ left: NODE_IDENT (name="a")
         └─ right: NODE_IDENT (name="b")
```

#### Struct: `struct Point { x: i64, y: i64 }`
```
NODE_STRUCT
├─ name: "Point"
└─ children: [Linked list of fields]
   ├─ NODE_FIELD_DECL (name="x", type=NODE_TYPE_I64) → next →
   └─ NODE_FIELD_DECL (name="y", type=NODE_TYPE_I64) → next → NULL
```

### 4.4 Program (Root Node)

```chronos
// Source:
// struct Point { x: i64, y: i64 }
// fn main() -> i64 { return 42; }
```

```
NODE_PROGRAM
└─ children: [Linked list of top-level declarations]
   ├─ NODE_STRUCT (name="Point") → next →
   └─ NODE_FUNCTION (name="main") → next → NULL
```

---

## 5. Funciones de Construcción del AST

### 5.1 Crear Nodos

```chronos
fn ast_alloc() -> *ASTNode {
    let node: *ASTNode = malloc(80);  // sizeof(ASTNode) = 80 bytes
    // Initialize all fields to 0/NULL
    node.node_type = 0;
    node.op = 0;
    node.left = 0;
    node.right = 0;
    node.body = 0;
    node.condition = 0;
    node.value = 0;
    node.name = 0;
    node.type_node = 0;
    node.children = 0;
    node.next = 0;
    node.line = 0;
    node.column = 0;
    return node;
}

fn ast_number(value: i64, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_NUMBER;
    node.value = value;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_ident(name: *i8, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_IDENT;
    node.name = name;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_binary_op(op: i64, left: *ASTNode, right: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_BINARY_OP;
    node.op = op;
    node.left = left;
    node.right = right;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_return(expr: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_RETURN;
    node.left = expr;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_let(name: *i8, type_node: *ASTNode, initializer: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_LET;
    node.name = name;
    node.type_node = type_node;
    node.right = initializer;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_function(name: *i8, params: *ASTNode, return_type: *ASTNode, body: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_FUNCTION;
    node.name = name;
    node.children = params;  // Linked list of parameters
    node.type_node = return_type;
    node.body = body;
    node.line = line;
    node.column = column;
    return node;
}

fn ast_block(statements: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_BLOCK;
    node.children = statements;  // Linked list of statements
    node.line = line;
    node.column = column;
    return node;
}

fn ast_if(condition: *ASTNode, then_block: *ASTNode, else_block: *ASTNode, line: i64, column: i64) -> *ASTNode {
    let node: *ASTNode = ast_alloc();
    node.node_type = NODE_IF;
    node.condition = condition;
    node.body = then_block;
    node.right = else_block;  // Can be NULL
    node.line = line;
    node.column = column;
    return node;
}
```

### 5.2 Linked Lists (Para params, fields, statements)

```chronos
fn ast_list_append(head: *ASTNode, new_node: *ASTNode) -> *ASTNode {
    // Append new_node to end of linked list
    // Returns new head

    if (head == 0) {
        // Empty list, new_node becomes head
        return new_node;
    }

    // Find last node
    let current: *ASTNode = head;
    while (current.next != 0) {
        current = current.next;
    }

    // Append
    current.next = new_node;
    return head;
}

fn ast_list_length(head: *ASTNode) -> i64 {
    let count: i64 = 0;
    let current: *ASTNode = head;
    while (current != 0) {
        count = count + 1;
        current = current.next;
    }
    return count;
}
```

---

## 6. Ejemplo de Construcción

### Source Code
```chronos
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
```

### Construcción del AST (paso a paso)

```chronos
// 1. Parse params
let param_a: *ASTNode = ast_alloc();
param_a.node_type = NODE_PARAM;
param_a.name = "a";
param_a.type_node = ast_type_i64();

let param_b: *ASTNode = ast_alloc();
param_b.node_type = NODE_PARAM;
param_b.name = "b";
param_b.type_node = ast_type_i64();

// Link params
param_a.next = param_b;
let params: *ASTNode = param_a;

// 2. Parse return type
let return_type: *ASTNode = ast_type_i64();

// 3. Parse body
let a_ref: *ASTNode = ast_ident("a", 2, 12);
let b_ref: *ASTNode = ast_ident("b", 2, 16);
let add_expr: *ASTNode = ast_binary_op(TOK_PLUS, a_ref, b_ref, 2, 14);
let return_stmt: *ASTNode = ast_return(add_expr, 2, 5);
let body: *ASTNode = ast_block(return_stmt, 1, 28);

// 4. Create function node
let func: *ASTNode = ast_function("add", params, return_type, body, 1, 1);
```

**Resultado:**
```
NODE_FUNCTION (name="add", line=1, col=1)
├─ children: [params]
│  ├─ NODE_PARAM (name="a", type=NODE_TYPE_I64)
│  └─ NODE_PARAM (name="b", type=NODE_TYPE_I64)
├─ type_node: NODE_TYPE_I64
└─ body: NODE_BLOCK
   └─ children: NODE_RETURN
      └─ left: NODE_BINARY_OP (op=TOK_PLUS)
         ├─ left: NODE_IDENT (name="a")
         └─ right: NODE_IDENT (name="b")
```

---

## 7. Recorrido del AST (Tree Walking)

### 7.1 Pretty Printer (Debug)

```chronos
fn ast_print(node: *ASTNode, indent: i64) -> i64 {
    if (node == 0) {
        return 0;
    }

    // Print indentation
    let i: i64 = 0;
    while (i < indent) {
        print("  ");
        i = i + 1;
    }

    // Print node type
    if (node.node_type == NODE_NUMBER) {
        print("NUMBER: ");
        print_int(node.value);
        println("");
    }
    if (node.node_type == NODE_IDENT) {
        print("IDENT: ");
        print(node.name);
        println("");
    }
    if (node.node_type == NODE_BINARY_OP) {
        print("BINARY_OP: ");
        print_int(node.op);
        println("");
        ast_print(node.left, indent + 1);
        ast_print(node.right, indent + 1);
    }
    if (node.node_type == NODE_RETURN) {
        println("RETURN:");
        ast_print(node.left, indent + 1);
    }
    if (node.node_type == NODE_FUNCTION) {
        print("FUNCTION: ");
        print(node.name);
        println("");
        println("  PARAMS:");
        ast_print_list(node.children, indent + 2);
        println("  BODY:");
        ast_print(node.body, indent + 2);
    }

    return 0;
}

fn ast_print_list(head: *ASTNode, indent: i64) -> i64 {
    let current: *ASTNode = head;
    while (current != 0) {
        ast_print(current, indent);
        current = current.next;
    }
    return 0;
}
```

### 7.2 Evaluator (Interpreter)

```chronos
fn ast_eval(node: *ASTNode) -> i64 {
    if (node == 0) {
        return 0;
    }

    if (node.node_type == NODE_NUMBER) {
        return node.value;
    }

    if (node.node_type == NODE_BINARY_OP) {
        let left_val: i64 = ast_eval(node.left);
        let right_val: i64 = ast_eval(node.right);

        if (node.op == TOK_PLUS) {
            return left_val + right_val;
        }
        if (node.op == TOK_MINUS) {
            return left_val - right_val;
        }
        if (node.op == TOK_STAR) {
            return left_val * right_val;
        }
        if (node.op == TOK_SLASH) {
            return left_val / right_val;
        }
    }

    return 0;
}
```

**Ejemplo:**
```chronos
// AST for: (10 + 20) * 2
let left_add: *ASTNode = ast_binary_op(TOK_PLUS, ast_number(10), ast_number(20));
let expr: *ASTNode = ast_binary_op(TOK_STAR, left_add, ast_number(2));

let result: i64 = ast_eval(expr);
print_int(result);  // Output: 60
```

---

## 8. Ventajas del AST Recursivo

### Problema Anterior (Flat Expr)
```chronos
struct Expr {
    op: i64,
    left: i64,   // ❌ Solo puede ser número
    right: i64   // ❌ Solo puede ser número
}
```

**Limitaciones:**
- ❌ No puede representar `(a + b) * (c + d)`
- ❌ No puede representar `p.x + p.y`
- ❌ No puede anidar expresiones
- ❌ No puede representar árboles profundos

### Solución Actual (Recursive AST)
```chronos
struct ASTNode {
    node_type: i64,
    left: *ASTNode,   // ✅ Puede ser CUALQUIER expresión
    right: *ASTNode   // ✅ Puede ser CUALQUIER expresión
}
```

**Capacidades:**
- ✅ Expresiones anidadas ilimitadas
- ✅ Field access en expresiones
- ✅ Function calls en expresiones
- ✅ Cualquier árbol arbitrariamente profundo
- ✅ Fácil de extender con nuevos tipos de nodos

---

## 9. Integración con Lexer

```chronos
fn parse_from_source(source: *i8) -> *ASTNode {
    // 1. Tokenize
    let tokens: *TokenList = lex_all(source);

    // 2. Parse tokens → AST
    let ast: *ASTNode = parse_program(tokens);

    return ast;
}
```

---

## 10. Memory Management

### Allocation
```chronos
// Each AST node: 80 bytes
// Small program (10 nodes): 800 bytes
// Medium program (100 nodes): 8 KB
// Large program (1000 nodes): 80 KB
```

### Deallocation (TODO for v1.0)
```chronos
fn ast_free(node: *ASTNode) -> i64 {
    if (node == 0) {
        return 0;
    }

    // Recursively free children
    ast_free(node.left);
    ast_free(node.right);
    ast_free(node.body);
    ast_free(node.condition);
    ast_free(node.type_node);
    ast_free(node.children);
    ast_free(node.next);

    // Free name string if allocated
    if (node.name != 0) {
        free(node.name);
    }

    // Free node itself
    free(node);
    return 0;
}
```

**Nota:** Para el compilador (proceso de corta duración), memory leaks son aceptables. El OS reclamará todo al terminar.

---

## 11. Próximos Pasos

Con el AST diseñado, ahora podemos:

1. **FASE 1:** Implementar el lexer que genera tokens
2. **FASE 2:** Implementar funciones de construcción del AST
3. **FASE 3:** Implementar el parser que convierte tokens → AST
4. **FASE 4:** Implementar codegen que convierte AST → assembly

---

**Próximo Documento:** `PARSER_DESIGN.md` - Recursive Descent Parser que construye el AST desde los tokens.
