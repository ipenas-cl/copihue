# Arquitectura del Compilador Chronos v1.0

**Versión:** 1.0
**Fecha:** 29 de octubre de 2025
**Estado:** Design Complete, Ready for Implementation

---

## 1. Visión General

### 1.1 Pipeline del Compilador

```
┌─────────────┐
│ Source Code │ (Chronos .ch file)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   LEXER     │ (Tokenization)
│             │ • Chars → Tokens
│             │ • Skip whitespace/comments
│             │ • Identify keywords
└──────┬──────┘
       │ Tokens (Token*)
       ▼
┌─────────────┐
│   PARSER    │ (Syntax Analysis)
│             │ • Tokens → AST
│             │ • Recursive descent
│             │ • Precedence climbing
└──────┬──────┘
       │ AST (ASTNode*)
       ▼
┌─────────────┐
│  ANALYZER   │ (Semantic Analysis) [FUTURE: v1.1]
│             │ • Type checking
│             │ • Symbol resolution
│             │ • Error detection
└──────┬──────┘
       │ Annotated AST
       ▼
┌─────────────┐
│  CODEGEN    │ (Code Generation)
│             │ • AST → x86-64 Assembly
│             │ • Register allocation
│             │ • Optimization
└──────┬──────┘
       │ Assembly (.asm)
       ▼
┌─────────────┐
│  ASSEMBLER  │ (Binary Generation)
│             │ • Assembly → Machine Code
│             │ • Symbol resolution
│             │ • Relocation
└──────┬──────┘
       │ Object Code (.o)
       ▼
┌─────────────┐
│   LINKER    │ (Linking)
│             │ • Objects → Executable
│             │ • Generate ELF64
│             │ • Entry point setup
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Executable  │ (./program)
└─────────────┘
```

### 1.2 Componentes Principales

| Componente | Archivo | Funciones Principales | Estado |
|------------|---------|----------------------|--------|
| **Lexer** | `lexer.ch` | `lex_all()`, `lex_next()` | DISEÑADO ✅ |
| **Parser** | `parser.ch` | `parse_program()`, `parse_expression()` | DISEÑADO ✅ |
| **AST** | `ast.ch` | `ast_alloc()`, `ast_binary_op()` | DISEÑADO ✅ |
| **Codegen** | `codegen.ch` | `codegen_program()`, `codegen_expr()` | TODO |
| **Assembler** | `toolchain.ch` | `assemble()` | EXISTE (needs upgrade) |
| **Linker** | `toolchain.ch` | `link()` | EXISTE (needs upgrade) |
| **Main** | `compiler.ch` | `main()`, orchestration | TODO |

---

## 2. Componente 1: LEXER

### Responsabilidad
Convertir source code (texto plano) en tokens.

### Input
```chronos
fn main() -> i64 { return 42; }
```

### Output
```
[TOK_FN, TOK_IDENT("main"), TOK_LPAREN, TOK_RPAREN, TOK_ARROW, TOK_I64, TOK_LBRACE, TOK_RETURN, TOK_NUMBER("42"), TOK_SEMICOLON, TOK_RBRACE, TOK_EOF]
```

### Estructuras
```chronos
struct Token {
    type: i64,      // TOK_* constant
    value: *i8,     // Token text
    line: i64,      // Source location
    column: i64
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
```

### API
```chronos
fn lexer_init(source: *i8) -> *Lexer
fn lex_next(lex: *Lexer) -> *Token
fn lex_all(source: *i8) -> *TokenList
```

**Ver:** `docs/LEXER_DESIGN.md` para detalles completos.

---

## 3. Componente 2: PARSER

### Responsabilidad
Convertir tokens en Abstract Syntax Tree (AST).

### Input
```
[TOK_RETURN, TOK_NUMBER("10"), TOK_PLUS, TOK_NUMBER("20"), TOK_SEMICOLON]
```

### Output
```
NODE_RETURN
└─ NODE_BINARY_OP(op=PLUS)
   ├─ NODE_NUMBER(10)
   └─ NODE_NUMBER(20)
```

### Estructuras
```chronos
struct ASTNode {
    node_type: i64,      // NODE_* constant
    op: i64,             // Operator type
    left: *ASTNode,      // Left child (recursive!)
    right: *ASTNode,     // Right child (recursive!)
    body: *ASTNode,      // Function/block body
    condition: *ASTNode, // If/while condition
    value: i64,          // Literal values
    name: *i8,           // Identifiers
    type_node: *ASTNode, // Type annotations
    children: *ASTNode,  // Linked list of children
    next: *ASTNode,      // Next in linked list
    line: i64,
    column: i64
}

struct Parser {
    tokens: *TokenList,
    pos: i64,
    current: *Token,
    previous: *Token
}
```

### API
```chronos
fn parser_init(tokens: *TokenList) -> *Parser
fn parse_program(tokens: *TokenList) -> *ASTNode
fn parse_expression(p: *Parser) -> *ASTNode
fn parse_statement(p: *Parser) -> *ASTNode
```

**Ver:** `docs/PARSER_DESIGN.md` y `docs/AST_DESIGN.md` para detalles completos.

---

## 4. Componente 3: CODEGEN

### Responsabilidad
Convertir AST en assembly x86-64.

### Input (AST)
```
NODE_BINARY_OP(op=PLUS)
├─ NODE_NUMBER(10)
└─ NODE_NUMBER(20)
```

### Output (Assembly)
```asm
    mov rax, 20        ; Right operand
    push rax
    mov rax, 10        ; Left operand
    pop rbx
    add rax, rbx       ; Result in rax
```

### Estructuras
```chronos
struct Codegen {
    output_buf: *i8,     // Assembly output buffer
    output_len: i64,
    output_cap: i64,

    // Code generation state
    label_counter: i64,   // For unique labels
    stack_offset: i64,    // Current stack position

    // Symbol table (for variables)
    var_names: [i8; 2048],      // Variable names
    var_offsets: [i64; 256],    // Stack offsets
    var_count: i64
}
```

### API
```chronos
fn codegen_init() -> *Codegen
fn codegen_program(cg: *Codegen, ast: *ASTNode) -> i64
fn codegen_function(cg: *Codegen, func_node: *ASTNode) -> i64
fn codegen_statement(cg: *Codegen, stmt: *ASTNode) -> i64
fn codegen_expression(cg: *Codegen, expr: *ASTNode) -> i64
fn emit(cg: *Codegen, line: *i8) -> i64
```

### Design Pattern: Tree Walking

```chronos
fn codegen_expression(cg: *Codegen, expr: *ASTNode) -> i64 {
    if (expr.node_type == NODE_NUMBER) {
        // Emit: mov rax, <number>
        let instr: [i8; 64];
        build_mov_imm(instr, expr.value);
        emit(cg, instr);
        return 0;
    }

    if (expr.node_type == NODE_BINARY_OP) {
        // Generate code for right operand first
        codegen_expression(cg, expr.right);
        emit(cg, "    push rax");

        // Generate code for left operand
        codegen_expression(cg, expr.left);
        emit(cg, "    pop rbx");

        // Generate operation
        if (expr.op == TOK_PLUS) {
            emit(cg, "    add rax, rbx");
        }
        if (expr.op == TOK_MINUS) {
            emit(cg, "    sub rax, rbx");
        }
        // ... etc

        return 0;
    }

    if (expr.node_type == NODE_IDENT) {
        // Load variable from stack
        let var_offset: i64 = lookup_variable(cg, expr.name);
        let instr: [i8; 64];
        build_mov_stack(instr, var_offset);
        emit(cg, instr);
        return 0;
    }

    if (expr.node_type == NODE_FIELD_ACCESS) {
        // Handle p.x
        // ... (complex, involves struct layout)
        return 0;
    }

    return 1;  // Unknown node type
}
```

---

## 5. Componente 4: ASSEMBLER/LINKER

### Responsabilidad
Convertir assembly text → ELF64 executable.

### Current State (toolchain.ch)
✅ Funciona para subset limitado de instrucciones
✅ Genera ELF64 válido
✅ Soporta ~40 instrucciones

### Needs Upgrade
- [ ] Soporte para más instrucciones
- [ ] Mejor manejo de símbolos
- [ ] Soporte para múltiples archivos objeto
- [ ] Mejor error reporting

**Ver:** `compiler/chronos/toolchain.ch` (ya existe)

---

## 6. Main Compiler Driver

### compiler.ch (Main Entry Point)

```chronos
fn main() -> i64 {
    println("Chronos Compiler v1.0");
    println("");

    // 1. Read command line args (TODO: for now hardcoded)
    let filename: *i8 = "/tmp/input.ch";

    // 2. Read source file
    println("Reading source...");
    let source: *i8 = read_file(filename);
    if (source == 0) {
        println("ERROR: Failed to read file");
        return 1;
    }

    // 3. Lexer: Source → Tokens
    println("Lexing...");
    let tokens: *TokenList = lex_all(source);
    if (tokens.count == 0) {
        println("ERROR: Lexing failed");
        return 1;
    }
    print("  ");
    print_int(tokens.count);
    println(" tokens");

    // 4. Parser: Tokens → AST
    println("Parsing...");
    let ast: *ASTNode = parse_program(tokens);
    if (ast == 0) {
        println("ERROR: Parsing failed");
        return 1;
    }
    println("  AST built successfully");

    // Optional: Print AST for debugging
    if (DEBUG_MODE == 1) {
        println("");
        println("AST:");
        ast_print(ast, 0);
        println("");
    }

    // 5. Codegen: AST → Assembly
    println("Generating code...");
    let cg: *Codegen = codegen_init();
    let gen_result: i64 = codegen_program(cg, ast);
    if (gen_result != 0) {
        println("ERROR: Code generation failed");
        return 1;
    }
    print("  ");
    print_int(cg.output_len);
    println(" bytes of assembly");

    // 6. Write assembly to file
    println("Writing assembly...");
    let asm_written: i64 = write_file("output.asm", cg.output_buf, cg.output_len);
    if (asm_written == 0) {
        println("ERROR: Failed to write assembly");
        return 1;
    }

    // 7. Assemble: Assembly → Object
    println("Assembling...");
    let asm_result: i64 = assemble("output.asm", "output.o");
    if (asm_result != 0) {
        println("ERROR: Assembly failed");
        return 1;
    }

    // 8. Link: Object → Executable
    println("Linking...");
    let link_result: i64 = link("output.o", "program");
    if (link_result != 0) {
        println("ERROR: Linking failed");
        return 1;
    }

    println("");
    println("✅ Compilation successful!");
    println("   Output: ./program");

    return 0;
}
```

---

## 7. Organización de Archivos

```
compiler/chronos/
├── compiler.ch           # Main driver (NEW)
├── lexer.ch             # Lexer implementation (NEW)
├── ast.ch               # AST structures and functions (NEW)
├── parser.ch            # Parser implementation (NEW)
├── codegen.ch           # Code generator (NEW)
├── toolchain.ch         # Assembler/Linker (EXISTS, upgrade)
├── typechecker.ch       # Type checker (FUTURE: v1.1)
│
├── archive/             # Old versions
│   └── pre-consolidation/
│       ├── compiler_main.ch
│       ├── compiler_v2.ch
│       └── compiler_v3.ch
│
└── README.md
```

### Tamaño Estimado

| Archivo | Lines of Code (estimated) | Purpose |
|---------|--------------------------|---------|
| `compiler.ch` | ~150 | Main driver, file I/O |
| `lexer.ch` | ~500 | Tokenization |
| `ast.ch` | ~300 | AST construction |
| `parser.ch` | ~800 | Recursive descent parser |
| `codegen.ch` | ~600 | Assembly generation |
| `toolchain.ch` | ~850 | Assembler/linker (exists) |
| **TOTAL** | **~3200** | Complete compiler |

---

## 8. Roadmap de Implementación

### FASE 1: Lexer (1 semana)
**Objetivo:** Implementar tokenización completa

**Tareas:**
- [ ] Crear `lexer.ch`
- [ ] Implementar `Token`, `TokenList`, `Lexer` structs
- [ ] Implementar `lex_next()` para todos los token types
- [ ] Implementar `lex_all()`
- [ ] Tests: números, identifiers, keywords, operators
- [ ] Compilar lexer con bootstrap compiler (chronos_v10)

**Entregable:**
```bash
./compiler/bootstrap-c/chronos_v10 lexer.ch
./chronos_program  # Should tokenize test input
```

### FASE 2: AST (1 semana)
**Objetivo:** Implementar construcción del AST

**Tareas:**
- [ ] Crear `ast.ch`
- [ ] Implementar `ASTNode` struct
- [ ] Implementar funciones de construcción: `ast_number()`, `ast_binary_op()`, etc.
- [ ] Implementar `ast_print()` para debugging
- [ ] Implementar `ast_eval()` (intérprete simple para testing)
- [ ] Tests: construir ASTs manualmente y evaluar

**Entregable:**
```bash
./compiler/bootstrap-c/chronos_v10 ast.ch
./chronos_program  # Should build and evaluate AST
```

### FASE 3: Parser (2 semanas)
**Objetivo:** Implementar parser recursive descent

**Tareas:**
- [ ] Crear `parser.ch`
- [ ] Implementar `Parser` struct
- [ ] Implementar parsing de expresiones (precedence climbing)
- [ ] Implementar parsing de statements
- [ ] Implementar parsing de declarations
- [ ] Tests: parsear programas reales y verificar AST
- [ ] Integración lexer → parser → AST

**Entregable:**
```bash
./compiler/bootstrap-c/chronos_v10 parser.ch
./chronos_program  # Should parse Chronos code into AST
```

### FASE 4: Codegen (2 semanas)
**Objetivo:** Implementar generación de código assembly

**Tareas:**
- [ ] Crear `codegen.ch`
- [ ] Implementar `Codegen` struct
- [ ] Implementar codegen para expresiones
- [ ] Implementar codegen para statements
- [ ] Implementar codegen para functions
- [ ] Symbol table para variables
- [ ] Tests: verificar assembly generado

**Entregable:**
```bash
./compiler/bootstrap-c/chronos_v10 codegen.ch
./chronos_program  # Should generate valid assembly from AST
```

### FASE 5: Integration (1 semana)
**Objetivo:** Integrar todos los componentes

**Tareas:**
- [ ] Crear `compiler.ch` (main driver)
- [ ] Integrar lexer → parser → codegen → assembler
- [ ] Command-line argument handling
- [ ] Better error reporting
- [ ] Tests end-to-end
- [ ] Compilar el compilador completo con bootstrap

**Entregable:**
```bash
./compiler/bootstrap-c/chronos_v10 compiler.ch
./chronos_program input.ch  # Compiles Chronos code!
```

### FASE 6: Self-Hosting (2 semanas)
**Objetivo:** Compilador se compila a sí mismo

**Tareas:**
- [ ] Reescribir `chronos_v10.c` en Chronos (usando el nuevo compilador)
- [ ] Three-stage bootstrap:
  - Stage 1: C compiler compiles Chronos compiler
  - Stage 2: Chronos compiler (from C) compiles itself
  - Stage 3: Chronos compiler (from Stage 2) compiles itself
  - Verify Stage 2 == Stage 3 (determinism)
- [ ] Eliminar dependencia de C

**Entregable:**
```bash
# Stage 1: Bootstrap
./compiler/bootstrap-c/chronos_v10 compiler.ch
mv chronos_program chronos_stage1

# Stage 2: Self-compile
./chronos_stage1 compiler.ch
mv chronos_program chronos_stage2

# Stage 3: Verify determinism
./chronos_stage2 compiler.ch
mv chronos_program chronos_stage3

# Compare binaries
diff chronos_stage2 chronos_stage3  # Should be identical!
```

---

## 9. Métricas de Éxito

### v1.0 Release Criteria

**Funcionalidad:**
- ✅ Lexer completo (todos los tokens)
- ✅ Parser completo (toda la gramática)
- ✅ AST recursivo (expresiones anidadas)
- ✅ Codegen funcional (assembly válido)
- ✅ Self-hosting (compila a sí mismo)
- ✅ Three-stage bootstrap determinístico

**Calidad:**
- ✅ Security: 9.8/10+ (mantener fixes de v0.18)
- ✅ Tests: 95%+ pass rate
- ✅ Documentation completa
- ✅ No memory leaks críticos (aceptable para compilador)

**Performance:**
- Compilación de archivo típico (500 líneas): < 500ms
- Memory usage: < 50 MB
- Binary size: < 500 KB

---

## 10. Comparación: Antes vs. Después

### ANTES (v0.17 - v0.19)

**Problemas:**
- ❌ Parser char-by-char (frágil)
- ❌ AST plano (no recursivo)
- ❌ No puede anidar expresiones
- ❌ Código duplicado (3 versiones)
- ❌ Hardcoded input files
- ❌ Sin command-line args
- ❌ Difícil de extender

**Código:**
```chronos
// Parsing char-by-char
while (source[i] == 108) {  // 'l'
    if (source[i+1] == 101) {  // 'e'
        if (source[i+2] == 116) {  // 't'
```

### DESPUÉS (v1.0)

**Soluciones:**
- ✅ Lexer robusto (token-based)
- ✅ AST recursivo (ilimitadamente anidable)
- ✅ Puede parsear cualquier expresión
- ✅ Un solo compilador unificado
- ✅ Command-line args
- ✅ Arquitectura modular
- ✅ Fácil de extender

**Código:**
```chronos
// Token-based parsing
if (parser_match(p, TOK_LET)) {
    return parse_let_statement(p);
}
```

---

## 11. Extensiones Futuras

### v1.1: Type System
- Type checker (semantic analysis)
- Type inference
- Generic types
- Better error messages

### v1.2: Optimizations
- Constant folding
- Dead code elimination
- Register allocation optimization
- Inline functions

### v1.3: Advanced Features
- Modules/imports
- Ownership system (Rust-like)
- Borrow checker
- Trait system

### v2.0: Backends
- LLVM backend
- ARM support
- WASM support
- JIT compilation

---

## 12. Conclusión

**FASE 0.2 COMPLETA** ✅

Hemos diseñado completamente la arquitectura del nuevo compilador Chronos v1.0:

1. **Lexer** - Tokenización robusta ✅
2. **Parser** - Recursive descent ✅
3. **AST** - Árbol recursivo ✅
4. **Codegen** - Generación de assembly ✅
5. **Integration** - Pipeline completo ✅

**Próximo Paso:** FASE 1 - Implementar el Lexer

**Tiempo estimado total:** 8-10 semanas para v1.0 completo

---

**Documentos de Referencia:**
- `docs/LEXER_DESIGN.md` - Diseño completo del lexer
- `docs/AST_DESIGN.md` - Diseño completo del AST
- `docs/PARSER_DESIGN.md` - Diseño completo del parser
- Este documento - Visión general e integración

**Firmado:**
- Chronos Architecture Team
- Fecha: 29 de octubre de 2025
- Estado: Ready for Implementation 🚀
