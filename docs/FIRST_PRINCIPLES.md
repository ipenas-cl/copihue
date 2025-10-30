# Chronos - Diseño desde Primeros Principios

**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Pregunta:** ¿Cómo DEBERÍA funcionar un lenguaje de programación?

---

## El Problema con el Approach Tradicional

Hemos estado discutiendo:
- ¿`fn` o `function`?
- ¿`set x = y` o `x = y`?
- ¿`++` o no `++`?

**Pero no hemos cuestionado:**
- ¿Por qué archivos de texto plano?
- ¿Por qué este pipeline de compilación?
- ¿Por qué esta separación usuario-compilador-OS?
- ¿Qué REALMENTE sirve al determinismo?

---

## Las 4 Capas Fundamentales

```
┌─────────────────────────────────────┐
│  1. INTERFAZ USUARIO                │  ¿Cómo escribe/edita código?
│     (Editor, IDE, REPL, Visual)     │
├─────────────────────────────────────┤
│  2. REPRESENTACIÓN                  │  ¿Cómo se almacena el programa?
│     (Texto, AST, Bytecode, DB)      │
├─────────────────────────────────────┤
│  3. TRANSFORMACIÓN                  │  ¿Cómo se traduce a ejecutable?
│     (Compiler pipeline)             │
├─────────────────────────────────────┤
│  4. MATERIALIZACIÓN                 │  ¿Cómo se ejecuta en el OS?
│     (Syscalls, ABI, Proceso)        │
└─────────────────────────────────────┘
```

Repensemos CADA capa.

---

## CAPA 1: Interfaz Usuario

### Opciones Tradicionales

**A) Archivos de Texto Plano** (C, Python, Rust)
```
editor (vim/vscode) → archivo.ch → compilador → ejecutable
```

**Ventajas:**
- ✅ Universal (cualquier editor)
- ✅ Control de versiones (git)
- ✅ Simple (solo texto)
- ✅ Herramientas existentes

**Desventajas:**
- ❌ Sintaxis necesaria (parsing)
- ❌ Errores de sintaxis
- ❌ Indentación/formato
- ❌ Edición no asistida

**B) REPL Interactivo** (Lisp, Python, OCaml)
```
>>> let x = 10
>>> let y = 20
>>> x + y
30
```

**Ventajas:**
- ✅ Feedback inmediato
- ✅ Exploración interactiva
- ✅ No "build step"

**Desventajas:**
- ❌ No para programas grandes
- ❌ Difícil versionar
- ❌ No reproducible

**C) IDE Estructurado** (Smalltalk, Unison)
```
No editas "texto", editas la ESTRUCTURA directamente
```

**Ventajas:**
- ✅ Sin errores de sintaxis (imposible!)
- ✅ Refactoring perfecto
- ✅ Autocompletado 100%
- ✅ Estructura siempre válida

**Desventajas:**
- ❌ Lock-in al IDE
- ❌ Curva de aprendizaje
- ❌ ¿Cómo versionar?

**D) Visual Programming** (Scratch, LabVIEW, Unreal Blueprints)
```
Bloques/nodos visuales, no texto
```

**Ventajas:**
- ✅ Muy accesible (niños)
- ✅ Flujo de datos visible
- ✅ Sin sintaxis

**Desventajas:**
- ❌ No escala a sistemas grandes
- ❌ Difícil para algoritmos complejos
- ❌ Verboso visualmente

**E) Literate Programming** (Org-mode, Jupyter, Quarto)
```markdown
# Mi Programa

Explicación en prosa...

```chronos
fn main() {
    // código
}
```

Más explicación...
```

**Ventajas:**
- ✅ Documentación integrada
- ✅ Narrativa clara
- ✅ Reproducible

**Desventajas:**
- ❌ Overhead de escritura
- ❌ Separación código-docs

---

### Propuesta Chronos: **Híbrido Progresivo**

**Nivel 1: Archivos de texto** (familiar, universal)
```chronos
// hello.ch
fn main() {
    print("Hello")
}
```

**Nivel 2: REPL integrado** (exploración)
```bash
$ chronos repl
>>> let x = 10
>>> let y = 20
>>> x + y
30
```

**Nivel 3: IDE estructurado (opcional)** (power users)
- Editas AST directamente
- Sintaxis generada automáticamente
- Refactoring perfecto
- Nunca errores de sintaxis

**Nivel 4: Time-travel debugging** (determinismo)
- Reproducción perfecta
- Replay de ejecuciones
- Bidirectional debugger

**Filosofía:**
- Empieza simple (texto)
- Crece en sofisticación
- No forzar una sola manera

---

## CAPA 2: Representación

### ¿Cómo almacenamos el programa?

**A) Texto Plano** (tradicional)
```
archivo.ch (UTF-8 text)
```

**Ventajas:**
- ✅ Git funciona
- ✅ Diff/patch funciona
- ✅ Herramientas existentes

**Desventajas:**
- ❌ Debe parsearse cada vez
- ❌ Sintaxis requerida
- ❌ Ambigüedades posibles

**B) AST Serializado** (Unison approach)
```
programa.ast (estructura binaria)
```

**Ventajas:**
- ✅ No parsing
- ✅ Siempre válido
- ✅ Más rápido

**Desventajas:**
- ❌ No human-readable
- ❌ Git no funciona bien
- ❌ Tooling complejo

**C) Database** (Smalltalk image)
```
Todo el programa en una DB
```

**Ventajas:**
- ✅ Incremental
- ✅ Queries potentes
- ✅ Versionado fino

**Desventajas:**
- ❌ No portable
- ❌ No estándar
- ❌ Lock-in

**D) Híbrido: Texto + Cache** (Rust, Zig approach)
```
archivo.ch (source)
.chronos/cache/archivo.ast (parsed cache)
.chronos/cache/archivo.ir (compiled cache)
```

**Ventajas:**
- ✅ Humano lee texto
- ✅ Compilador usa cache
- ✅ Incremental compilation
- ✅ Best of both worlds

---

### Propuesta Chronos: **Texto + Cache Incremental**

```
proyecto/
├── src/
│   ├── main.ch              # Texto (humano)
│   └── lib.ch               # Texto (humano)
│
└── .chronos/
    ├── cache/
    │   ├── main.ast         # AST parseado
    │   ├── main.typed       # AST tipado
    │   ├── main.ir          # IR optimizado
    │   └── main.asm         # Assembly
    │
    └── metadata/
        ├── dependencies     # Grafo de deps
        └── checksums        # Para invalidación
```

**Beneficios:**
- ✅ Source es texto (git, diff, universal)
- ✅ Compilador es incremental (rápido)
- ✅ No recompila innecesariamente
- ✅ Cache es reproducible (determinista!)

**Compilación incremental:**
```bash
$ chronos build main.ch
Parsing main.ch...          [cache miss]
Parsing lib.ch...           [cache miss]
Type checking...            [cache miss]
Generating code...          [cache miss]
Done. (2.5s)

$ # Cambio main.ch
$ chronos build main.ch
Parsing main.ch...          [cache miss]
Parsing lib.ch...           [cache HIT]   ← No reparsea
Type checking...            [incremental]  ← Solo main.ch
Generating code...          [incremental]
Done. (0.3s)                               ← 8x más rápido
```

---

## CAPA 3: Transformación (Compiler Pipeline)

### Pipeline Tradicional

```
Source Code (archivo.ch)
    ↓
[LEXER] ────────────────→ Tokens
    ↓
[PARSER] ───────────────→ AST (Abstract Syntax Tree)
    ↓
[SEMANTIC ANALYSIS] ────→ Typed AST
    ↓                     (Symbol table, Type checking)
[IR GENERATION] ────────→ Intermediate Representation
    ↓
[OPTIMIZATION] ─────────→ Optimized IR
    ↓                     (Dead code, inlining, etc)
[CODE GENERATION] ──────→ Assembly
    ↓
[ASSEMBLER] ────────────→ Object Code (.o)
    ↓
[LINKER] ───────────────→ Executable
```

### Problemas con Pipeline Tradicional

1. **No incremental** - Recompila todo cada vez
2. **Fases separadas** - Información se pierde entre fases
3. **No interactivo** - Esperás que termine
4. **Errores tardíos** - Type error después de parsing completo
5. **No queryable** - No podés preguntar "¿qué tipo tiene esto?"

---

### Pipelines Alternativos

**A) Salsa (Rust-analyzer approach)**
```
Query-based incremental compilation

fn parse_file(file: FileId) -> AST {
    // Cacheable, incremental
}

fn type_check(ast: AST) -> TypedAST {
    // Solo recomputa si input cambió
}

fn compile(ast: TypedAST) -> IR {
    // Incremental
}
```

**Ventajas:**
- ✅ Incremental automático
- ✅ Cachea resultados
- ✅ Paralelizable
- ✅ Queryable (IDE features)

**B) Tree-sitter (Incremental parsing)**
```
Edit: cambio 1 línea
    ↓
Parser: solo re-parsea nodo afectado (no todo el archivo)
    ↓
AST actualizado incrementalmente
```

**Ventajas:**
- ✅ Parsing en <1ms
- ✅ IDE responsivo
- ✅ Error recovery

**C) JIT Compilation (Julia, Java)**
```
Bytecode interpretado inicialmente
    ↓
Hot paths detectadas
    ↓
JIT compila a machine code
```

**Ventajas:**
- ✅ Startup rápido
- ✅ Optimización runtime
- ✅ Adaptive

**Desventajas:**
- ❌ No determinista! (diferentes runs, diferente compilación)

---

### Propuesta Chronos: **Query-Based Incremental Pipeline**

**Arquitectura:**
```rust
// Cada fase es una "query" cacheable

@query
fn parse(file: FileId) -> Result<AST, ParseError> {
    // Solo se ejecuta si file cambió
    // Resultado cacheado
}

@query
fn resolve_imports(ast: AST) -> Result<ResolvedAST, ImportError> {
    // Depende de parse()
    // Incremental
}

@query
fn type_check(ast: ResolvedAST) -> Result<TypedAST, TypeError> {
    // Depende de resolve_imports()
    // Solo re-ejecuta si input cambió
}

@query
fn generate_ir(ast: TypedAST) -> IR {
    // Incremental
}

@query
fn optimize(ir: IR) -> OptimizedIR {
    // Cacheable
}

@query
fn codegen(ir: OptimizedIR) -> Assembly {
    // Determinista!
}
```

**Beneficios:**

1. **Incremental automático**
   ```bash
   $ chronos build        # Primera vez: 5s
   $ # Edit 1 line
   $ chronos build        # Segunda vez: 0.1s
   ```

2. **IDE features gratis**
   ```
   IDE pregunta: "¿Qué tipo tiene esta variable?"
      ↓
   Compiler: type_check(ast) [cached] → resultado instantáneo
   ```

3. **Paralelizable**
   ```
   parse(file1) ║ parse(file2) ║ parse(file3)
        ↓              ↓              ↓
        └──────────────┴──────────────┘
                       ↓
                  type_check()
   ```

4. **Determinista**
   - Mismo source → mismo cache
   - Mismo AST → mismo IR
   - Mismo IR → mismo assembly
   - Reproducible perfectamente

---

## CAPA 4: Materialización en OS

### ¿Cómo ejecutamos el programa?

**A) Ejecutable Nativo** (C, Rust approach)
```
Chronos → Assembly → Machine Code → ELF64 → ./programa
```

**Ventajas:**
- ✅ Máximo performance
- ✅ No runtime overhead
- ✅ Deploy simple (un binario)

**Desventajas:**
- ❌ Platform-specific
- ❌ No portable
- ❌ Link-time final

**B) Bytecode + VM** (Java, Python approach)
```
Chronos → Bytecode → VM ejecuta
```

**Ventajas:**
- ✅ Portable
- ✅ Sandbox posible
- ✅ JIT posible

**Desventajas:**
- ❌ Runtime overhead
- ❌ VM complexity
- ❌ No determinista (GC, JIT)

**C) WebAssembly** (Modern approach)
```
Chronos → WASM → Runtime
```

**Ventajas:**
- ✅ Portable
- ✅ Sandbox
- ✅ Fast
- ✅ Standard

**Desventajas:**
- ❌ Limited syscalls
- ❌ No direct OS access

**D) Unikernel** (MirageOS approach)
```
Chronos → Kernel incluido → Boots directly
```

**Ventajas:**
- ✅ Minimal attack surface
- ✅ Fast boot
- ✅ Determinista

**Desventajas:**
- ❌ Complex deployment
- ❌ Limited use cases

---

### Propuesta Chronos: **Multi-Target Output**

```
Chronos Source
    ↓
[Compiler]
    ↓
┌───┴────┬────────┬────────┬──────────┐
│        │        │        │          │
Native   WASM   Bytecode  Kernel    IR
(x64)                              (para otros)
```

**Targets soportados:**

1. **Native x86-64** (Linux, macOS, Windows)
   ```bash
   $ chronos build --target x86_64-linux
   $ ./programa
   ```

2. **WASM** (Web, Edge, Serverless)
   ```bash
   $ chronos build --target wasm32
   $ wasmtime programa.wasm
   ```

3. **LLVM IR** (interop con LLVM ecosystem)
   ```bash
   $ chronos build --emit llvm-ir
   $ clang programa.ll -o programa
   ```

4. **Bytecode** (portable, debuggable)
   ```bash
   $ chronos build --target bytecode
   $ chronos-vm programa.bc
   ```

---

## Integración: El Pipeline Completo

```
┌──────────────────────────────────────────────────────────┐
│ USUARIO                                                  │
│                                                          │
│  editor.ch ←─→ LSP ←─→ chronos-language-server          │
│  (VSCode/Vim)          (IDE features)                    │
└────────────────────┬─────────────────────────────────────┘
                     │ save
                     ↓
┌──────────────────────────────────────────────────────────┐
│ REPRESENTACIÓN                                           │
│                                                          │
│  archivo.ch (texto UTF-8)                                │
│       ↓                                                  │
│  .chronos/cache/                                         │
│       ├── archivo.ast      [cached]                      │
│       ├── archivo.typed    [cached]                      │
│       └── archivo.ir       [cached]                      │
└────────────────────┬─────────────────────────────────────┘
                     │ chronos build
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TRANSFORMACIÓN (Query-based Incremental)                 │
│                                                          │
│  parse()          [incremental, cached]                  │
│      ↓                                                   │
│  resolve()        [incremental, cached]                  │
│      ↓                                                   │
│  type_check()     [incremental, cached]                  │
│      ↓                                                   │
│  borrow_check()   [incremental, cached]                  │
│      ↓                                                   │
│  generate_ir()    [incremental, cached]                  │
│      ↓                                                   │
│  optimize()       [deterministic]                        │
│      ↓                                                   │
│  codegen()        [deterministic]                        │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────┐
│ MATERIALIZACIÓN                                          │
│                                                          │
│  Assembly (.asm)                                         │
│      ↓                                                   │
│  Object (.o)                                             │
│      ↓                                                   │
│  Executable (ELF64)                                      │
│      ↓                                                   │
│  OS Process                                              │
│      ├── Memory layout (stack, heap, .text, .data)       │
│      ├── Syscalls (read, write, open, mmap, ...)        │
│      └── ABI (System V AMD64)                            │
└──────────────────────────────────────────────────────────┘
```

---

## Características Únicas de Chronos

### 1. **Determinismo End-to-End**

```
Mismo source.ch
    ↓
Mismo AST (parsing determinista)
    ↓
Mismo IR (codegen determinista)
    ↓
Mismo assembly (layout determinista)
    ↓
Mismo ejecutable (bit-for-bit reproducible)
    ↓
Misma ejecución (no undefined behavior)
```

**Verificable:**
```bash
$ chronos build programa.ch
$ sha256sum programa
abc123...

$ # En otra máquina
$ chronos build programa.ch
$ sha256sum programa
abc123...                    # Idéntico!
```

### 2. **Incremental Todo**

```
Edit 1 línea
    ↓
Re-parse solo ese archivo [0.1ms]
    ↓
Re-type-check solo dependientes [1ms]
    ↓
Re-codegen solo afectado [10ms]
    ↓
Re-link [50ms]
    ↓
Total: ~60ms (no 5 segundos)
```

### 3. **Queryable Compiler**

```bash
# ¿Qué tipo tiene esta expresión?
$ chronos query type main.ch:10:5
i32

# ¿Dónde se usa esta función?
$ chronos query references main.ch:add
main.ch:20:10
lib.ch:15:3

# ¿Cuál es el WCET de esta función?
$ chronos query wcet main.ch:sensor_read
150 cycles (worst case)
```

### 4. **Time-Travel Debugging**

```bash
$ chronos debug --record programa
Recording execution... Done.

$ chronos replay programa.trace
> break main:10
> run
Breakpoint at main:10
> print x
42
> reverse-step          # ← Ir ATRÁS en el tiempo
> print x
41
```

**Posible porque:**
- Determinismo → replay perfecto
- Todas las syscalls grabadas
- Todo input/output reproducible

### 5. **Multi-Target Sin Cambios**

```bash
# Same source, multiple targets
$ chronos build --target x86_64-linux
$ chronos build --target wasm32
$ chronos build --target aarch64-linux

# Mismo comportamiento, diferentes plataformas
```

---

## Ejemplo Concreto: "Hello World"

### 1. Usuario escribe

```chronos
// hello.ch
fn main() {
    print("Hello, World!")
}
```

### 2. Representación

```
hello.ch (UTF-8 text)
.chronos/cache/hello.ast (parsed, cached)
.chronos/cache/hello.typed (type-checked, cached)
```

### 3. Compilación (primera vez)

```bash
$ chronos build hello.ch

[1/6] Parsing hello.ch...               [1ms]
[2/6] Resolving imports...              [0ms]
[3/6] Type checking...                  [2ms]
[4/6] Borrow checking...                [1ms]
[5/6] Generating code...                [10ms]
[6/6] Linking...                        [50ms]

✓ Build succeeded (64ms)
```

### 4. Compilación (segunda vez, sin cambios)

```bash
$ chronos build hello.ch

✓ All cached, nothing to do (0ms)
```

### 5. Compilación (edit 1 línea)

```bash
$ chronos build hello.ch

[1/6] Parsing hello.ch...               [1ms] (re-parsed)
[2/6] Resolving imports...              [cached]
[3/6] Type checking...                  [1ms] (incremental)
[4/6] Borrow checking...                [1ms] (incremental)
[5/6] Generating code...                [5ms] (incremental)
[6/6] Linking...                        [10ms] (incremental)

✓ Build succeeded (18ms)
```

### 6. Ejecución

```bash
$ ./hello
Hello, World!

$ echo $?
0
```

### 7. Materialización (bajo el capó)

```
ELF64 Executable "hello"
├── .text section (código)
│   └── main:
│       pushq %rbp
│       movq  %rsp, %rbp
│       leaq  .Lstr(%rip), %rdi
│       call  print
│       xorq  %rax, %rax
│       popq  %rbp
│       ret
│
├── .rodata section (datos)
│   └── .Lstr: "Hello, World!\0"
│
└── .dynamic (símbolos para linker)

OS ejecuta:
1. Load ELF en memoria
2. Map .text como executable
3. Map .rodata como read-only
4. Jump a _start → main
5. main llama print (syscall write)
6. Return 0
7. OS limpia proceso
```

---

## Decisiones de Diseño para Determinismo

### 1. **Orden de Evaluación Definido**

```chronos
// Siempre left-to-right
let x = f() + g()    // f() primero, luego g()

// No existe
f(i++, i++)          // ERROR: no existe ++
```

### 2. **No Undefined Behavior**

```chronos
// Overflow: panic o wrap (configurable)
let x: i32 = 2147483647
let y = x + 1        // panic (debug) o -2147483648 (release con wrapping explícito)

// Bounds checking
let arr = [1, 2, 3]
let x = arr[5]       // panic: index out of bounds
```

### 3. **Deterministic Layout**

```chronos
struct Point {
    x: i32,    // offset 0
    y: i32,    // offset 4
}
// Siempre 8 bytes, no padding random
```

### 4. **Reproducible Builds**

```bash
$ chronos build --deterministic
# Garantía: mismo source → mismo binario (bit-for-bit)
# No timestamps, no ASLR, layout fijo
```

### 5. **Observable Side Effects**

```chronos
// Syscalls explícitos
fn read_file(path: &str) -> Result[String, IOError]
    effects: IO        // Anotación explícita
{
    // ...
}

// Pure functions (sin effects)
fn add(a: i32, b: i32) -> i32
    pure               // Garantizado sin side effects
{
    a + b
}
```

---

## Roadmap de Implementación

### Fase 1: Texto + Pipeline Básico (2 meses)
- [x] Archivos de texto (.ch)
- [ ] Lexer
- [ ] Parser → AST
- [ ] Type checker (básico)
- [ ] Codegen (x86-64)
- [ ] Compile "Hello World"

### Fase 2: Incremental + Cache (1 mes)
- [ ] Query-based compilation
- [ ] Salsa integration
- [ ] AST caching
- [ ] Incremental type checking

### Fase 3: IDE Support (1 mes)
- [ ] Language Server Protocol (LSP)
- [ ] VSCode extension
- [ ] Syntax highlighting
- [ ] Auto-completion
- [ ] Go-to-definition

### Fase 4: Multi-Target (2 meses)
- [ ] WASM backend
- [ ] LLVM IR backend
- [ ] Cross-compilation
- [ ] Target-specific optimizations

### Fase 5: Debugging (1 mes)
- [ ] Time-travel debugging
- [ ] Record/replay
- [ ] Deterministic replay

### Fase 6: Advanced Features (3 meses)
- [ ] WCET analysis
- [ ] Effect system
- [ ] Formal verification hooks

---

## Comparación: Chronos vs Otros

| Feature | C | Rust | Go | Zig | Chronos |
|---------|---|------|-----|-----|---------|
| Incremental compile | ❌ | ✅ | ✅ | ✅ | ✅ |
| Reproducible builds | ❌ | ✅ | ❌ | ✅ | ✅ |
| Query compiler | ❌ | ✅ | ❌ | ❌ | ✅ |
| Time-travel debug | ❌ | ❌ | ❌ | ❌ | ✅ |
| Multi-target | ✅ | ✅ | ❌ | ✅ | ✅ |
| Deterministic exec | ❌ | ✅ | ❌ | ✅ | ✅ |
| LSP support | ✅ | ✅ | ✅ | ✅ | ✅ |
| WASM target | ✅ | ✅ | ✅ | ❌ | ✅ |

---

## Conclusión

**No estamos eligiendo sintaxis, estamos diseñando un SISTEMA completo:**

1. **Interfaz:** Texto + REPL + IDE opcional
2. **Representación:** Texto + cache incremental
3. **Transformación:** Query-based, incremental, determinista
4. **Materialización:** Multi-target (native, WASM, bytecode)

**Filosofía:**
- Determinismo en CADA capa
- Incremental en CADA fase
- Queryable en TODO momento
- Reproducible SIEMPRE

**Próxima pregunta:**
¿Qué decidimos sobre cada capa?

---

**Decisiones pendientes:**

A) **Sintaxis:** ¿Texto tradicional o algo más estructurado?
B) **Build system:** ¿Makefile-like o integrated?
C) **Package manager:** ¿Desde día 1 o después?
D) **Standard library:** ¿Qué incluir?
E) **Tooling:** ¿Prioridad en qué?

---

**¿Qué repensamos primero?**
