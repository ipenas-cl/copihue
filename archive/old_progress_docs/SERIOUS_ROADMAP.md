# 🎯 CHRONOS SERIOUS ROADMAP
## Objetivo: 100% Self-Hosted, Un Solo Compilador

**Autor:** Ignacio Peña
**Fecha Inicio:** 29 de octubre, 2025
**Estimación Total:** 3-4 meses de trabajo intenso
**Status:** 🔴 EN PLANIFICACIÓN

---

## 🎖️ OBJETIVO FINAL

```
Estado Actual:
❌ 3 compiladores duplicados
❌ Dependencia del bootstrap en C
❌ Parser primitivo (char-by-char)
❌ No hay AST recursivo
❌ Memory leaks en todos lados

Estado Objetivo (v1.0):
✅ 1 solo compilador profesional
✅ 100% escrito en Chronos
✅ Se compila a sí mismo SIN C
✅ Parser recursive descent
✅ AST completo y recursivo
✅ Memory management serio (arena allocator)
✅ Type system funcional
```

---

## 📊 DIAGNÓSTICO TÉCNICO

### Capacidades Actuales de Chronos

**Lo que Chronos v0.18 SÍ puede hacer:**
- ✅ Structs con fields
- ✅ Pointers y arrays
- ✅ Funciones con parámetros
- ✅ Arithmetic expressions (limitado)
- ✅ Control flow básico (if, while)
- ✅ File I/O
- ✅ malloc/free

**Lo que Chronos v0.18 NO puede hacer (pero necesita):**
- ❌ Expresiones complejas recursivas: `(a + b) * (c + d)`
- ❌ Function calls en expresiones: `foo(bar(x))`
- ❌ Arrays de structs: `let points: [Point; 10]`
- ❌ String literals largos
- ❌ Enums
- ❌ Pattern matching
- ❌ Closures

### Gap Analysis

Para que Chronos compile su propio código (compiler_v3.ch), necesita:

```chronos
// NECESARIO 1: Arrays de structs
let g_struct_names: [i8; 1024];  // ✅ Ya soportado

// NECESARIO 2: Múltiples variables locales
let i: i64 = 0;
let max_len: i64 = 256;
let found: i64 = 0;  // ✅ Ya soportado

// NECESARIO 3: Nested expressions
result = (value * 10) + (ch - 48);  // ❌ NO SOPORTADO

// NECESARIO 4: Complex control flow
while (condition1 && condition2) {
    if (nested) {
        while (inner) { }  // ✅ Parcialmente soportado
    }
}

// NECESARIO 5: Function calls en expresiones
let result: i64 = parse_expr(find_token(source, 0));  // ❌ NO SOPORTADO
```

**Conclusión:** Estamos ~70% del camino. Necesitamos el 30% restante.

---

## 🗓️ ROADMAP DETALLADO

### 📅 FASE 0: PREPARACIÓN (Semana 1-2)

**Objetivo:** Un solo compilador limpio como base

#### Tareas:

**0.1 Consolidación** ⏱️ 2 días
- [ ] Analizar diferencias entre compiler_main, v2, v3
- [ ] Elegir v3 como base (más completo)
- [ ] Mover v2 y main a archive/
- [ ] Renombrar v3 → chronos_compiler.ch
- [ ] Actualizar todos los scripts para usar el nuevo nombre
- [ ] Verificar que tests siguen pasando

**0.2 Análisis de Features Necesarias** ⏱️ 2 días
- [ ] Listar todas las features que usa chronos_compiler.ch
- [ ] Marcar cuáles ya funcionan
- [ ] Identificar gaps críticos
- [ ] Crear issues para cada gap

**0.3 Arquitectura del Nuevo Compilador** ⏱️ 3 días
- [ ] Diseñar estructura de módulos
- [ ] Definir interfaces entre componentes
- [ ] Decidir formato del AST
- [ ] Documentar decisiones de diseño

**Entregables Fase 0:**
- ✅ Un solo archivo: `chronos_compiler.ch`
- ✅ Documento: `COMPILER_ARCHITECTURE.md`
- ✅ Lista de features pendientes

---

### 📅 FASE 1: LEXER REAL (Semana 3-4)

**Objetivo:** Tokenizer robusto que reemplace el char-by-char parsing

#### Diseño del Lexer

```chronos
// Token types
let TOKEN_IDENT: i64 = 1;
let TOKEN_NUMBER: i64 = 2;
let TOKEN_STRING: i64 = 3;
let TOKEN_LPAREN: i64 = 4;
let TOKEN_RPAREN: i64 = 5;
let TOKEN_LBRACE: i64 = 6;
// ... etc

struct Token {
    type: i64,
    lexeme: [i8; 64],    // El texto del token
    line: i64,
    column: i64,
    value: i64           // Para números
}

struct Lexer {
    source: *i8,
    pos: i64,
    line: i64,
    column: i64,
    tokens: [Token; 1024],  // Array de tokens
    token_count: i64
}

fn lex(source: *i8) -> *Lexer {
    // Convierte "fn main() -> i64 { return 42; }"
    // en tokens: [FN, IDENT, LPAREN, RPAREN, ARROW, ...]
}
```

#### Tareas:

**1.1 Implementar Token struct** ⏱️ 1 día
- [ ] Definir todos los token types (30+)
- [ ] Crear Token struct
- [ ] Crear Lexer struct

**1.2 Implementar Lexer básico** ⏱️ 3 días
- [ ] Keywords (fn, let, if, while, return, struct)
- [ ] Identifiers (nombres de variables)
- [ ] Numbers (literales numéricos)
- [ ] Operators (+, -, *, /, ==, !=, etc)
- [ ] Delimiters ({}, (), [], ;, :)

**1.3 Error Handling** ⏱️ 2 días
- [ ] Mensajes con línea y columna
- [ ] Caracteres inválidos
- [ ] Strings sin cerrar

**1.4 Tests** ⏱️ 1 día
- [ ] 20+ tests de lexer
- [ ] Edge cases
- [ ] Error cases

**Entregables Fase 1:**
- ✅ Lexer funcional
- ✅ 20+ tests passing
- ✅ Error messages útiles

---

### 📅 FASE 2: AST RECURSIVO (Semana 5-6)

**Objetivo:** Árbol de sintaxis abstracta real que soporte recursión

#### Diseño del AST

```chronos
// Node types
let NODE_PROGRAM: i64 = 1;
let NODE_FUNCTION: i64 = 2;
let NODE_BLOCK: i64 = 3;
let NODE_RETURN: i64 = 4;
let NODE_BINARY_OP: i64 = 5;
let NODE_CALL: i64 = 6;
let NODE_LITERAL: i64 = 7;

struct ASTNode {
    type: i64,

    // Para literales
    value: i64,

    // Para identificadores
    name: [i8; 64],

    // Para operadores binarios
    op: i64,
    left: *ASTNode,   // ← RECURSIVO!
    right: *ASTNode,  // ← RECURSIVO!

    // Para funciones
    params: *ASTNode,     // Lista enlazada de params
    body: *ASTNode,       // Block
    return_type: i64,

    // Lista enlazada para hermanos
    next: *ASTNode
}

// Ejemplo: (a + b) * c
// Se representa como:
//       *
//      / \
//     +   c
//    / \
//   a   b
```

#### Tareas:

**2.1 Definir ASTNode** ⏱️ 2 días
- [ ] Struct con todos los campos necesarios
- [ ] Funciones helper para crear nodos
- [ ] Memory management strategy (arena?)

**2.2 Implementar constructores** ⏱️ 2 días
- [ ] `ast_literal(value)`
- [ ] `ast_binary(op, left, right)`
- [ ] `ast_call(name, args)`
- [ ] `ast_function(name, params, body)`

**2.3 Pretty printer** ⏱️ 1 día
- [ ] Función para imprimir el árbol
- [ ] Útil para debugging

**2.4 Tests** ⏱️ 1 día
- [ ] Construir ASTs manualmente
- [ ] Verificar estructura
- [ ] Pretty print

**Entregables Fase 2:**
- ✅ AST recursivo completo
- ✅ Constructores para todos los tipos
- ✅ Pretty printer para debugging

---

### 📅 FASE 3: PARSER RECURSIVE DESCENT (Semana 7-9)

**Objetivo:** Parser profesional que construye el AST

#### Diseño del Parser

```chronos
struct Parser {
    lexer: *Lexer,
    current: i64,      // Índice del token actual
    had_error: i64
}

// Gramática (simplificada):
// program     → function*
// function    → "fn" IDENT "(" params? ")" "->" type block
// block       → "{" statement* "}"
// statement   → return_stmt | let_stmt | expr_stmt | if_stmt | while_stmt
// return_stmt → "return" expression ";"
// expression  → equality
// equality    → comparison ( ("==" | "!=") comparison )*
// comparison  → term ( (">" | "<" | ">=" | "<=") term )*
// term        → factor ( ("+" | "-") factor )*
// factor      → unary ( ("*" | "/") unary )*
// unary       → ("!" | "-") unary | call
// call        → primary ( "(" arguments? ")" | "." IDENT )*
// primary     → NUMBER | IDENT | "(" expression ")"

fn parse_expression(p: *Parser) -> *ASTNode {
    return parse_equality(p);
}

fn parse_equality(p: *Parser) -> *ASTNode {
    let left: *ASTNode = parse_comparison(p);

    while (match(p, TOKEN_EQUAL_EQUAL) || match(p, TOKEN_BANG_EQUAL)) {
        let op: i64 = previous(p).type;
        let right: *ASTNode = parse_comparison(p);
        left = ast_binary(op, left, right);
    }

    return left;
}
```

#### Tareas:

**3.1 Parser base** ⏱️ 2 días
- [ ] Parser struct
- [ ] Helper functions (advance, peek, match, consume)
- [ ] Error recovery

**3.2 Expressions** ⏱️ 5 días
- [ ] Primary (números, identificadores)
- [ ] Call expressions (función())
- [ ] Unary (-, !)
- [ ] Binary operators (+, -, *, /)
- [ ] Comparisons (==, !=, <, >)
- [ ] Logic (&&, ||)
- [ ] Field access (struct.field)

**3.3 Statements** ⏱️ 4 días
- [ ] Let declarations
- [ ] Return statements
- [ ] If statements
- [ ] While loops
- [ ] Blocks

**3.4 Top-level** ⏱️ 2 días
- [ ] Function definitions
- [ ] Struct definitions
- [ ] Global variables

**3.5 Tests** ⏱️ 2 días
- [ ] 50+ parser tests
- [ ] Edge cases
- [ ] Error recovery

**Entregables Fase 3:**
- ✅ Parser completo
- ✅ Construye AST para todo Chronos
- ✅ 50+ tests passing

---

### 📅 FASE 4: CODEGEN MEJORADO (Semana 10-11)

**Objetivo:** Generar assembly desde el AST

#### Diseño del Codegen

```chronos
struct CodegenContext {
    output: *i8,
    offset: i64,

    // Symbol table
    locals: [Local; 256],
    local_count: i64,

    // Stack management
    stack_offset: i64,

    // Label generation
    label_count: i64
}

struct Local {
    name: [i8; 64],
    offset: i64,      // Stack offset
    type: i64
}

fn codegen(ctx: *CodegenContext, node: *ASTNode) -> i64 {
    if (node.type == NODE_BINARY_OP) {
        // Recursivo!
        codegen(ctx, node.left);
        emit(ctx, "    push rax");
        codegen(ctx, node.right);
        emit(ctx, "    pop rbx");

        if (node.op == OP_ADD) {
            emit(ctx, "    add rax, rbx");
        }
        // etc...
    }
    // ... otros casos
}
```

#### Tareas:

**4.1 Codegen base** ⏱️ 2 días
- [ ] CodegenContext struct
- [ ] Symbol table básica
- [ ] Emit helpers

**4.2 Expressions** ⏱️ 4 días
- [ ] Literals
- [ ] Variables (load/store)
- [ ] Binary operations
- [ ] Function calls
- [ ] Field access

**4.3 Statements** ⏱️ 3 días
- [ ] Let declarations
- [ ] Return
- [ ] If/else
- [ ] While loops

**4.4 Functions** ⏱️ 2 días
- [ ] Prologue/epilogue
- [ ] Parameters
- [ ] Local variables

**4.5 Tests** ⏱️ 2 días
- [ ] E2E: Parse → Codegen → Assemble → Run
- [ ] Verificar salidas correctas

**Entregables Fase 4:**
- ✅ Codegen funcional
- ✅ Genera assembly correcto
- ✅ Tests E2E passing

---

### 📅 FASE 5: REESCRIBIR BOOTSTRAP (Semana 12-14)

**Objetivo:** Compilador inicial escrito en Chronos

#### El Bootstrap Problem

```
Problema:
- Necesitamos un binario de Chronos para compilar Chronos
- Pero no tenemos uno (solo el C)

Solución (Bootstrapping de 3 etapas):

Etapa 0 (Una vez):
1. Compilar chronos_compiler.ch con el bootstrap en C
2. Esto genera: chronos_stage1

Etapa 1:
3. Compilar chronos_compiler.ch con chronos_stage1
4. Esto genera: chronos_stage2

Etapa 2:
5. Compilar chronos_compiler.ch con chronos_stage2
6. Esto genera: chronos_stage3

Verificación:
7. stage2 y stage3 deben ser idénticos (byte-by-byte)
8. Si lo son: ✅ BOOTSTRAP COMPLETO
9. Si no: ❌ Hay un bug

Una vez completo:
- stage3 se convierte en el compilador oficial
- Nunca más necesitas C
```

#### Tareas:

**5.1 Verificar que chronos_compiler.ch funciona** ⏱️ 3 días
- [ ] Compilar con bootstrap C
- [ ] Verificar todos los tests pasan
- [ ] Benchmarks

**5.2 Compilar chronos_compiler.ch con sí mismo** ⏱️ 2 días
- [ ] Stage 1
- [ ] Stage 2
- [ ] Stage 3

**5.3 Verificación** ⏱️ 2 días
- [ ] Comparar binarios stage2 vs stage3
- [ ] Si difieren, debug
- [ ] Iterar hasta convergencia

**5.4 Automatización** ⏱️ 1 día
- [ ] Script para bootstrap completo
- [ ] CI/CD setup

**5.5 Documentación** ⏱️ 2 días
- [ ] Proceso de bootstrap
- [ ] Cómo contribuir sin bootstrap C

**Entregables Fase 5:**
- ✅ chronos_stage3 (binario auto-compilado)
- ✅ Script de bootstrap
- ✅ Documentación completa

---

### 📅 FASE 6: LIMPIEZA Y RELEASE (Semana 15-16)

**Objetivo:** v1.0 production-ready

#### Tareas:

**6.1 Eliminar código viejo** ⏱️ 2 días
- [ ] Archivar compiler_v2, compiler_main
- [ ] Archivar bootstrap en C (como referencia histórica)
- [ ] Limpiar archivos duplicados

**6.2 Documentación final** ⏱️ 3 días
- [ ] README actualizado
- [ ] ARCHITECTURE.md completo
- [ ] CONTRIBUTING.md para nuevos devs
- [ ] Examples actualizados

**6.3 Performance tuning** ⏱️ 2 días
- [ ] Benchmarks
- [ ] Optimizaciones básicas
- [ ] Memory profiling

**6.4 Tests completos** ⏱️ 3 días
- [ ] 100+ tests
- [ ] Fuzzing básico
- [ ] CI passing

**6.5 Release** ⏱️ 1 día
- [ ] Tag v1.0.0
- [ ] Release notes
- [ ] Anuncio

**Entregables Fase 6:**
- ✅ Chronos v1.0 - 100% self-hosted
- ✅ Un solo compilador
- ✅ Production-ready

---

## 📊 RECURSOS NECESARIOS

### Tiempo
- **Mínimo:** 3 meses (fulltime)
- **Realista:** 4 meses (fulltime)
- **Con trabajo:** 6-8 meses (part-time)

### Skills Necesarias
- ✅ Ya tienes: Entiendes compiladores
- ✅ Ya tienes: Conoces x86-64 assembly
- ✅ Ya tienes: Chronos syntax
- 🟡 Necesitas: Parser techniques (libro recomendado abajo)
- 🟡 Necesitas: AST traversal patterns

### Libros/Recursos Recomendados
1. **"Crafting Interpreters" by Bob Nystrom** - ESENCIAL para parser
2. **"Engineering a Compiler" by Cooper & Torczon** - Referencia técnica
3. **"Writing Compilers and Interpreters" by Mak** - Práctico

---

## 🎯 MILESTONES CLAVES

### Milestone 1: Un Solo Compilador (Semana 2)
- chronos_compiler.ch consolidado
- Tests pasando
- Arquitectura documentada

### Milestone 2: Lexer + AST (Semana 6)
- Tokenizer funcional
- AST recursivo completo
- Pretty printer

### Milestone 3: Parser Completo (Semana 9)
- Parse todo el lenguaje
- Construye AST correcto
- 50+ tests

### Milestone 4: Codegen (Semana 11)
- Assembly desde AST
- E2E funcionando
- Tests passing

### Milestone 5: Bootstrap (Semana 14)
- Stage2 == Stage3
- Sin dependencia de C
- ✅ 100% SELF-HOSTED

### Milestone 6: v1.0 Release (Semana 16)
- Código limpio
- Docs completas
- Production-ready

---

## ⚠️ RIESGOS Y MITIGACIONES

### Riesgo 1: Chronos no tiene capacidad suficiente
**Probabilidad:** Media
**Impacto:** Alto
**Mitigación:**
- Agregar features faltantes antes del bootstrap
- Simplificar el compilador si es necesario

### Riesgo 2: Bugs en el parser son difíciles de debug
**Probabilidad:** Alta
**Impacto:** Medio
**Mitigación:**
- Pretty printer para AST
- Tests exhaustivos
- Comparar con parser en C

### Riesgo 3: Bootstrap no converge
**Probabilidad:** Media
**Impacto:** Alto
**Mitigación:**
- Determinismo garantizado en codegen
- Comparación byte-by-byte
- Tests de no-regresión

### Riesgo 4: Toma más tiempo del estimado
**Probabilidad:** Alta
**Impacto:** Bajo (si no hay deadline)
**Mitigación:**
- Milestones claros
- Celebrar progreso incremental
- No apresurarse

---

## 🏆 ÉXITO SE VE ASÍ

```bash
# Compilar Chronos sin C:
./chronos compile chronos_compiler.ch -o chronos_new

# Verificar que el nuevo compila igual:
./chronos_new compile chronos_compiler.ch -o chronos_verify

# Comparar (deben ser idénticos):
diff chronos_new chronos_verify
# No output = ✅ ÉXITO

# Archivar el bootstrap en C:
mv compiler/bootstrap-c archive/historical/

# Un solo compilador:
ls compiler/chronos/
# chronos_compiler.ch  ← UN SOLO ARCHIVO
```

---

## 💬 PREGUNTAS FRECUENTES

**Q: ¿Por qué no usar LLVM?**
A: Fase 7 (post-v1.0). Primero necesitamos self-hosting.

**Q: ¿Y si falla el bootstrap?**
A: Tenemos el C como fallback. No hay riesgo.

**Q: ¿Cuándo empezamos?**
A: AHORA. Fase 0.1 toma 2 días.

**Q: ¿Puedo ayudar?**
A: Sí, pero primero necesito consolidar la base.

**Q: ¿Es realmente factible?**
A: Sí. TCC lo hizo. Go lo hizo. Rust lo hizo. Nosotros también podemos.

---

## 🚀 PRÓXIMO PASO

**AHORA MISMO:**
1. Leer este documento completo
2. Confirmar que quieres hacer esto
3. Empezar Fase 0.1: Consolidación

**¿Listo para empezar?**

Responde "SÍ" y empezamos con Fase 0.1 AHORA.
