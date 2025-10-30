# Chronos AST Format Specification v2.0

**Fecha:** 29 de octubre de 2025
**Propósito:** Especificación completa del Abstract Syntax Tree

---

## 1. PRINCIPIOS DE DISEÑO

### 1.1 Objetivos

- ✅ **Memoria eficiente** - Layout compacto, alignment óptimo
- ✅ **Type-safe** - Cada node tiene tipo explícito
- ✅ **Extensible** - Fácil agregar nuevos node types
- ✅ **Arena allocation** - Todos los nodes en un arena para lifetime management
- ✅ **Zero-copy** - Referencias directas sin duplicación

### 1.2 Representación en Memoria

Todos los nodes se almacenan en un **Arena Allocator**:
```
+-----------------+
| Arena           |
|  [Node 1]      |
|  [Node 2]      |
|  [Node 3]      |
|  ...           |
+-----------------+
```

**NodeId:** Offset dentro del arena (i64)
- Permite referencias sin punteros
- Fácil serialización
- Determinista

---

## 2. NODE TYPES

### 2.1 Base Node Structure

```
struct ASTNode {
    node_type: NodeType,    // 8 bytes (i64)
    span: Span,             // 16 bytes (2 x i64)
    flags: NodeFlags,       // 8 bytes (i64)
    data: NodeData,         // Variable (union-like)
}

Total: 32 bytes + sizeof(NodeData)
```

**NodeType (enum):**
```
0x0000_0000: Invalid
0x0001_0000: Literal group
0x0002_0000: Expression group
0x0003_0000: Statement group
0x0004_0000: Declaration group
0x0005_0000: Pattern group
0x0006_0000: Type group
```

**Span (source location):**
```
struct Span {
    start: i64,  // Byte offset en source file
    end: i64,    // Byte offset en source file
}
```

**NodeFlags (bit flags):**
```
0x01: Is_Const     - Expression es constante
0x02: Has_Error    - Node tiene error semántico
0x04: Is_Mut       - Variable es mutable
0x08: Is_Unsafe    - Dentro de unsafe block
0x10: Is_Async     - Función async
0x20: Is_RT        - RT path (no heap allocation)
```

---

## 3. LITERAL NODES

### 3.1 Integer Literal

```
NodeType: 0x0001_0001

struct IntegerLiteral {
    value: i128,        // 16 bytes (soporte hasta i128)
    suffix: TypeSuffix, // 8 bytes (i32, u64, etc.)
    base: NumericBase,  // 8 bytes (decimal=10, hex=16, bin=2, oct=8)
}

Total size: 32 bytes
```

**Ejemplo AST:**
```chronos
let x = 42;
```
```
IntegerLiteral {
    value: 42,
    suffix: None,
    base: Decimal(10),
}
```

### 3.2 Float Literal

```
NodeType: 0x0001_0002

struct FloatLiteral {
    value: f64,         // 8 bytes
    suffix: FloatType,  // 8 bytes (f32, f64)
    _padding: [u8; 16], // 16 bytes padding
}

Total size: 32 bytes
```

### 3.3 String Literal

```
NodeType: 0x0001_0003

struct StringLiteral {
    string_id: StringId, // 8 bytes (offset en string table)
    length: i64,         // 8 bytes
    encoding: Encoding,  // 8 bytes (UTF8, ASCII)
    _padding: [u8; 8],   // 8 bytes
}

Total size: 32 bytes
```

**String Table (separado del AST):**
```
String Table
├─ [0]: "hello"
├─ [5]: "world"
├─ [10]: "foo"
└─ ...
```

### 3.4 Boolean Literal

```
NodeType: 0x0001_0004

struct BoolLiteral {
    value: bool,        // 1 byte
    _padding: [u8; 31], // 31 bytes padding
}

Total size: 32 bytes
```

### 3.5 Character Literal

```
NodeType: 0x0001_0005

struct CharLiteral {
    value: u32,         // 4 bytes (Unicode scalar)
    encoding: Encoding, // 8 bytes
    _padding: [u8; 20], // 20 bytes
}

Total size: 32 bytes
```

---

## 4. EXPRESSION NODES

### 4.1 Binary Operation

```
NodeType: 0x0002_0001

struct BinaryOp {
    op: BinaryOpKind,   // 8 bytes
    lhs: NodeId,        // 8 bytes (left operand)
    rhs: NodeId,        // 8 bytes (right operand)
    result_type: TypeId,// 8 bytes
}

Total size: 32 bytes
```

**BinaryOpKind:**
```
enum BinaryOpKind {
    // Arithmetic
    Add = 0x01,
    Sub = 0x02,
    Mul = 0x03,
    Div = 0x04,
    Mod = 0x05,

    // Bitwise
    BitAnd = 0x10,
    BitOr = 0x11,
    BitXor = 0x12,
    Shl = 0x13,
    Shr = 0x14,

    // Comparison
    Eq = 0x20,
    Neq = 0x21,
    Lt = 0x22,
    Gt = 0x23,
    Lte = 0x24,
    Gte = 0x25,

    // Logical
    And = 0x30,
    Or = 0x31,

    // Assignment
    Assign = 0x40,
    AddAssign = 0x41,
    // ...
}
```

**Ejemplo AST:**
```chronos
let result = a + b;
```
```
BinaryOp {
    op: Add,
    lhs: NodeId(a_variable),
    rhs: NodeId(b_variable),
    result_type: TypeId(i32),
}
```

### 4.2 Unary Operation

```
NodeType: 0x0002_0002

struct UnaryOp {
    op: UnaryOpKind,    // 8 bytes
    operand: NodeId,    // 8 bytes
    result_type: TypeId,// 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

**UnaryOpKind:**
```
enum UnaryOpKind {
    Neg = 0x01,      // -x
    Not = 0x02,      // !x
    BitNot = 0x03,   // ~x
    Deref = 0x04,    // *x
    Ref = 0x05,      // &x
    RefMut = 0x06,   // &mut x
}
```

### 4.3 Function Call

```
NodeType: 0x0002_0003

struct FunctionCall {
    func: NodeId,       // 8 bytes (function being called)
    args: NodeList,     // 8 bytes (list of arguments)
    result_type: TypeId,// 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

**NodeList (separado):**
```
struct NodeList {
    start: i64,   // Index en node_list_storage
    count: i64,   // Number of items
}

node_list_storage: [NodeId; MAX_NODES]
```

### 4.4 Variable Reference

```
NodeType: 0x0002_0004

struct VarRef {
    symbol_id: SymbolId,// 8 bytes (index en symbol table)
    var_type: TypeId,   // 8 bytes
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

### 4.5 Field Access

```
NodeType: 0x0002_0005

struct FieldAccess {
    base: NodeId,       // 8 bytes (struct instance)
    field_name: StringId,// 8 bytes
    field_index: i64,   // 8 bytes (offset en struct)
    field_type: TypeId, // 8 bytes
}

Total size: 32 bytes
```

### 4.6 Array Index

```
NodeType: 0x0002_0006

struct ArrayIndex {
    array: NodeId,      // 8 bytes
    index: NodeId,      // 8 bytes
    element_type: TypeId,// 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

### 4.7 Cast Expression

```
NodeType: 0x0002_0007

struct Cast {
    expr: NodeId,       // 8 bytes
    from_type: TypeId,  // 8 bytes
    to_type: TypeId,    // 8 bytes
    cast_kind: CastKind,// 8 bytes
}

Total size: 32 bytes

enum CastKind {
    NoOp,           // No operation (types equal)
    IntToInt,       // Integer widening/truncation
    FloatToFloat,   // Float conversion
    IntToFloat,
    FloatToInt,
    PtrToPtr,       // Pointer cast
    Transmute,      // Unsafe transmute
}
```

### 4.8 Match Expression

```
NodeType: 0x0002_0008

struct Match {
    scrutinee: NodeId,  // 8 bytes (expression being matched)
    arms: NodeList,     // 8 bytes (list of MatchArm)
    result_type: TypeId,// 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes

struct MatchArm {
    pattern: NodeId,    // Pattern to match
    guard: NodeId,      // Optional guard (if let ... = ...)
    body: NodeId,       // Expression when matched
}
```

---

## 5. STATEMENT NODES

### 5.1 Let Statement

```
NodeType: 0x0003_0001

struct LetStmt {
    pattern: NodeId,    // 8 bytes (variable name or pattern)
    type_ann: TypeId,   // 8 bytes (optional type annotation)
    init: NodeId,       // 8 bytes (initializer expression)
    is_mut: bool,       // 1 byte
    _padding: [u8; 7],  // 7 bytes
}

Total size: 32 bytes
```

**Ejemplo:**
```chronos
let x: i32 = 42;
```
```
LetStmt {
    pattern: Identifier("x"),
    type_ann: TypeId(i32),
    init: IntegerLiteral(42),
    is_mut: false,
}
```

### 5.2 Expression Statement

```
NodeType: 0x0003_0002

struct ExprStmt {
    expr: NodeId,       // 8 bytes
    has_semicolon: bool,// 1 byte
    _padding: [u8; 23], // 23 bytes
}

Total size: 32 bytes
```

### 5.3 Return Statement

```
NodeType: 0x0003_0003

struct ReturnStmt {
    value: NodeId,      // 8 bytes (optional)
    _padding: [u8; 24], // 24 bytes
}

Total size: 32 bytes
```

### 5.4 Block

```
NodeType: 0x0003_0004

struct Block {
    stmts: NodeList,    // 8 bytes
    expr: NodeId,       // 8 bytes (optional final expression)
    scope_id: ScopeId,  // 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

### 5.5 If Statement

```
NodeType: 0x0003_0005

struct If {
    condition: NodeId,  // 8 bytes
    then_block: NodeId, // 8 bytes
    else_block: NodeId, // 8 bytes (optional)
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

### 5.6 While Loop

```
NodeType: 0x0003_0006

struct While {
    condition: NodeId,  // 8 bytes
    body: NodeId,       // 8 bytes
    label: StringId,    // 8 bytes (optional)
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

### 5.7 For Loop

```
NodeType: 0x0003_0007

struct For {
    pattern: NodeId,    // 8 bytes (loop variable)
    iterator: NodeId,   // 8 bytes (iterable)
    body: NodeId,       // 8 bytes
    label: StringId,    // 8 bytes
}

Total size: 32 bytes
```

### 5.8 Loop (infinite)

```
NodeType: 0x0003_0008

struct Loop {
    body: NodeId,       // 8 bytes
    label: StringId,    // 8 bytes
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

### 5.9 Break/Continue

```
NodeType: 0x0003_0009 / 0x0003_000A

struct BreakContinue {
    label: StringId,    // 8 bytes (optional)
    value: NodeId,      // 8 bytes (for break with value)
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

---

## 6. DECLARATION NODES

### 6.1 Function Declaration

```
NodeType: 0x0004_0001

struct FunctionDecl {
    name: StringId,     // 8 bytes
    params: NodeList,   // 8 bytes
    return_type: TypeId,// 8 bytes
    body: NodeId,       // 8 bytes (Block or null for extern)
    flags: FnFlags,     // 8 bytes
    _padding: [u8; 0],  // 0 bytes (already 40)
}

Total size: 40 bytes

struct FnParam {
    name: StringId,
    param_type: TypeId,
    is_mut: bool,
}

enum FnFlags {
    None = 0x00,
    Extern = 0x01,
    Unsafe = 0x02,
    Async = 0x04,
    Const = 0x08,
    Inline = 0x10,
    NoInline = 0x20,
}
```

**Ejemplo:**
```chronos
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```
```
FunctionDecl {
    name: "add",
    params: [
        FnParam { name: "a", param_type: i32, is_mut: false },
        FnParam { name: "b", param_type: i32, is_mut: false },
    ],
    return_type: i32,
    body: Block(...),
    flags: None,
}
```

### 6.2 Struct Declaration

```
NodeType: 0x0004_0002

struct StructDecl {
    name: StringId,     // 8 bytes
    fields: NodeList,   // 8 bytes
    repr: StructRepr,   // 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes

struct StructField {
    name: StringId,
    field_type: TypeId,
    offset: i64,        // Byte offset en struct
    is_pub: bool,
}

enum StructRepr {
    Chronos,     // Default layout
    C,           // C-compatible
    Packed,      // No padding
    Align(i64),  // Specific alignment
}
```

### 6.3 Enum Declaration

```
NodeType: 0x0004_0003

struct EnumDecl {
    name: StringId,     // 8 bytes
    variants: NodeList, // 8 bytes
    repr: EnumRepr,     // 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes

struct EnumVariant {
    name: StringId,
    fields: NodeList,   // Empty for unit variants
    discriminant: i64,  // Optional explicit value
}

enum EnumRepr {
    Default,
    U8, U16, U32, U64,
    I8, I16, I32, I64,
}
```

### 6.4 Type Alias

```
NodeType: 0x0004_0004

struct TypeAlias {
    name: StringId,     // 8 bytes
    target: TypeId,     // 8 bytes
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

### 6.5 Trait Declaration

```
NodeType: 0x0004_0005

struct TraitDecl {
    name: StringId,     // 8 bytes
    methods: NodeList,  // 8 bytes
    supertraits: NodeList,// 8 bytes
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

### 6.6 Impl Block

```
NodeType: 0x0004_0006

struct ImplBlock {
    trait_ref: TypeId,  // 8 bytes (or null for inherent impl)
    self_type: TypeId,  // 8 bytes
    items: NodeList,    // 8 bytes (methods)
    _padding: [u8; 8],  // 8 bytes
}

Total size: 32 bytes
```

---

## 7. PATTERN NODES

### 7.1 Identifier Pattern

```
NodeType: 0x0005_0001

struct IdentPattern {
    name: StringId,     // 8 bytes
    is_mut: bool,       // 1 byte
    _padding: [u8; 23], // 23 bytes
}

Total size: 32 bytes
```

### 7.2 Wildcard Pattern

```
NodeType: 0x0005_0002

struct WildcardPattern {
    _padding: [u8; 32], // 32 bytes
}

Total size: 32 bytes
```

### 7.3 Tuple Pattern

```
NodeType: 0x0005_0003

struct TuplePattern {
    elements: NodeList, // 8 bytes
    _padding: [u8; 24], // 24 bytes
}

Total size: 32 bytes
```

### 7.4 Struct Pattern

```
NodeType: 0x0005_0004

struct StructPattern {
    struct_type: TypeId,// 8 bytes
    fields: NodeList,   // 8 bytes (field patterns)
    is_exhaustive: bool,// 1 byte
    _padding: [u8; 15], // 15 bytes
}

Total size: 32 bytes

struct FieldPattern {
    field_name: StringId,
    pattern: NodeId,
}
```

---

## 8. TYPE NODES

### 8.1 Primitive Type

```
NodeType: 0x0006_0001

struct PrimitiveType {
    kind: PrimitiveKind,// 8 bytes
    _padding: [u8; 24], // 24 bytes
}

Total size: 32 bytes

enum PrimitiveKind {
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    Bool,
    Char,
    Str,
    Unit,
    Never,
}
```

### 8.2 Reference Type

```
NodeType: 0x0006_0002

struct ReferenceType {
    inner: TypeId,      // 8 bytes
    is_mut: bool,       // 1 byte
    lifetime: LifetimeId,// 8 bytes
    _padding: [u8; 15], // 15 bytes
}

Total size: 32 bytes
```

### 8.3 Pointer Type

```
NodeType: 0x0006_0003

struct PointerType {
    inner: TypeId,      // 8 bytes
    is_mut: bool,       // 1 byte
    _padding: [u8; 23], // 23 bytes
}

Total size: 32 bytes
```

### 8.4 Array Type

```
NodeType: 0x0006_0004

struct ArrayType {
    element: TypeId,    // 8 bytes
    size: i64,          // 8 bytes (or -1 for unknown)
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

### 8.5 Slice Type

```
NodeType: 0x0006_0005

struct SliceType {
    element: TypeId,    // 8 bytes
    _padding: [u8; 24], // 24 bytes
}

Total size: 32 bytes
```

### 8.6 Tuple Type

```
NodeType: 0x0006_0006

struct TupleType {
    elements: TypeList, // 8 bytes
    _padding: [u8; 24], // 24 bytes
}

Total size: 32 bytes
```

### 8.7 Function Type

```
NodeType: 0x0006_0007

struct FunctionType {
    params: TypeList,   // 8 bytes
    return_type: TypeId,// 8 bytes
    _padding: [u8; 16], // 16 bytes
}

Total size: 32 bytes
```

---

## 9. AST STORAGE

### 9.1 Arena Layout

```
struct AST {
    // Node storage
    nodes: Vec<ASTNode>,        // All nodes

    // Auxiliary storage
    node_lists: Vec<NodeId>,    // Lists of nodes
    type_lists: Vec<TypeId>,    // Lists of types
    strings: String,            // String table

    // Indexing
    root: NodeId,               // Root node
    source_file: PathBuf,       // Source file path
}
```

### 9.2 Node Allocation

```
// Allocate new node
fn alloc_node(ast: &mut AST, node: ASTNode) -> NodeId {
    let id = ast.nodes.len();
    ast.nodes.push(node);
    return id as NodeId;
}

// Get node
fn get_node(ast: &AST, id: NodeId) -> &ASTNode {
    return &ast.nodes[id as usize];
}
```

### 9.3 String Interning

```
struct StringTable {
    storage: String,            // All strings concatenated
    offsets: Vec<(usize, usize)>,// (start, length) pairs
    map: HashMap<&str, StringId>,// Deduplication
}

fn intern_string(table: &mut StringTable, s: &str) -> StringId {
    if let Some(&id) = table.map.get(s) {
        return id; // Already interned
    }

    let id = table.offsets.len();
    let start = table.storage.len();
    table.storage.push_str(s);
    let length = s.len();
    table.offsets.push((start, length));
    table.map.insert(&table.storage[start..start+length], id);
    return id as StringId;
}
```

---

## 10. TRAVERSAL PATTERNS

### 10.1 Visitor Pattern

```
trait ASTVisitor {
    fn visit_expr(&mut self, node: NodeId);
    fn visit_stmt(&mut self, node: NodeId);
    fn visit_decl(&mut self, node: NodeId);
    // ...
}

fn walk_ast(ast: &AST, visitor: &mut dyn ASTVisitor, node: NodeId) {
    match get_node(ast, node).node_type {
        NodeType::BinaryOp(op) => {
            visitor.visit_expr(op.lhs);
            visitor.visit_expr(op.rhs);
        }
        NodeType::Block(block) => {
            for stmt in block.stmts {
                visitor.visit_stmt(stmt);
            }
        }
        // ...
    }
}
```

### 10.2 Pretty Printing

```
fn print_ast(ast: &AST, node: NodeId, indent: usize) {
    let spaces = " ".repeat(indent);
    match get_node(ast, node) {
        BinaryOp { op, lhs, rhs, .. } => {
            println!("{}BinaryOp({})", spaces, op);
            print_ast(ast, lhs, indent + 2);
            print_ast(ast, rhs, indent + 2);
        }
        // ...
    }
}
```

---

## 11. EXAMPLE AST

### Source Code
```chronos
fn factorial(n: i32) -> i32 {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}
```

### AST Representation
```
FunctionDecl {
    name: "factorial",
    params: [FnParam { name: "n", type: i32 }],
    return_type: i32,
    body: Block {
        stmts: [
            If {
                condition: BinaryOp {
                    op: Lte,
                    lhs: VarRef("n"),
                    rhs: IntegerLiteral(1),
                },
                then_block: Block {
                    stmts: [ReturnStmt { value: IntegerLiteral(1) }]
                },
                else_block: null,
            },
            ReturnStmt {
                value: BinaryOp {
                    op: Mul,
                    lhs: VarRef("n"),
                    rhs: FunctionCall {
                        func: VarRef("factorial"),
                        args: [BinaryOp {
                            op: Sub,
                            lhs: VarRef("n"),
                            rhs: IntegerLiteral(1),
                        }],
                    },
                },
            },
        ],
    },
}
```

---

## 12. MEMORY LAYOUT SUMMARY

### Node Sizes
- All nodes: **32 bytes** (cache-friendly)
- Alignment: **8 bytes**
- Total arena overhead: **~10%**

### Typical Program
```
1000 lines of code:
- ~5000 AST nodes
- ~160 KB node storage
- ~20 KB string table
- ~10 KB auxiliary storage
Total: ~190 KB
```

---

## 13. NEXT STEPS

- [ ] Implement arena allocator
- [ ] Implement node constructors
- [ ] Implement visitor pattern
- [ ] Implement pretty printer
- [ ] Write tests for each node type

---

**Firmado:** Chronos Compiler Team
**Versión:** 2.0.0-spec
**Estado:** Ready for implementation
