# Chronos - Plan de Implementación INMEDIATO

**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Estado:** ¡COMENZAMOS!

---

## Objetivo: Milestone 1 - "Hello World" en 2 Semanas

**Meta:** Compilar un programa "Hello World" desde template Chronos hasta ejecutable.

```chronos
Program: hello
  Actions:
    - Print "Hello, World!"
```

↓ **Chronos Compiler** ↓

```
$ ./hello
Hello, World!
$ echo $?
0
```

---

## Decisiones Técnicas

### 1. Lenguaje del Compiler: **Rust**

**¿Por qué Rust?**
- ✅ Necesitamos lo que predicamos (memory safety, determinismo)
- ✅ Excelente para parsers (nom, pest, lalrpop)
- ✅ Type system fuerte (ideal para template validation)
- ✅ Zero-cost abstractions
- ✅ Cargo (build system, testing, benchmarks)
- ✅ LSP support (rust-analyzer)

**Alternativas consideradas:**
- ❌ Python: demasiado lento, no determinista
- ❌ C/C++: no memory safe, justamente lo que evitamos
- ❌ Go: GC = no determinista
- ✅ Zig: viable, pero menos ecosistema para parsers

### 2. Parser: **YAML + Custom Validator**

**¿Por qué YAML?**
- ✅ Templates ya parecen YAML naturalmente
- ✅ Parsers robustos (serde_yaml)
- ✅ Familiar para usuarios
- ✅ Fácil de extender

**Ejemplo:**
```yaml
# hello.chronos
Program:
  name: hello
  description: Hello world program

  actions:
    - Print: "Hello, World!"
```

### 3. Code Generation: **Directo a Assembly x86-64**

**Milestone 1:** Generar assembly AT&T syntax
**Milestone 2:** Generar ejecutable ELF64

```asm
.global main
.type main, @function

main:
    # Write syscall
    movq $1, %rax           # syscall: write
    movq $1, %rdi           # fd: stdout
    leaq msg(%rip), %rsi    # buffer
    movq $14, %rdx          # count
    syscall

    # Exit syscall
    movq $60, %rax          # syscall: exit
    xorq %rdi, %rdi         # status: 0
    syscall

msg:
    .ascii "Hello, World!\n"
```

### 4. Build Pipeline

```
Template (.chronos)
    ↓
[YAML Parser] (serde_yaml)
    ↓
[Template Validator]
    ↓
[Code Generator]
    ↓
Assembly (.asm)
    ↓
[Assembler] (GNU as)
    ↓
Object (.o)
    ↓
[Linker] (ld)
    ↓
Executable
```

**Fase 1:** Usar GNU `as` y `ld`
**Fase 2:** Escribir assembler/linker propio

---

## Milestone 1: Implementación (2 Semanas)

### Semana 1: Parser + Validator

**Día 1-2: Setup del Proyecto**
```bash
cargo new chronos-compiler
cd chronos-compiler

# Dependencies
cargo add serde --features derive
cargo add serde_yaml
cargo add anyhow
cargo add thiserror
```

**Estructura:**
```
chronos-compiler/
├── Cargo.toml
├── src/
│   ├── main.rs              # CLI
│   ├── parser/
│   │   ├── mod.rs
│   │   └── template.rs      # Template AST
│   ├── validator/
│   │   ├── mod.rs
│   │   └── rules.rs         # Validation rules
│   ├── codegen/
│   │   ├── mod.rs
│   │   └── asm_x64.rs       # x86-64 assembly generation
│   └── lib.rs
├── tests/
│   ├── parser_tests.rs
│   └── integration_tests.rs
└── examples/
    └── hello.chronos
```

**Día 3-4: Template AST**

```rust
// src/parser/template.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize)]
#[serde(tag = "type")]
pub enum Template {
    Program(ProgramTemplate),
    Function(FunctionTemplate),
    // Más templates después...
}

#[derive(Debug, Deserialize, Serialize)]
pub struct ProgramTemplate {
    pub name: String,
    pub description: Option<String>,
    pub actions: Vec<Action>,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Action {
    Print(String),
    // Más acciones después...
}

#[derive(Debug, Deserialize, Serialize)]
pub struct FunctionTemplate {
    pub name: String,
    pub description: Option<String>,
    pub inputs: Option<Vec<Parameter>>,
    pub output: Option<TypeSpec>,
    pub implementation: Vec<Statement>,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct Parameter {
    pub name: String,
    #[serde(rename = "type")]
    pub param_type: TypeSpec,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum TypeSpec {
    I32,
    I64,
    String,
    // Más tipos después...
}

#[derive(Debug, Deserialize, Serialize)]
pub enum Statement {
    Return(Expression),
    // Más statements después...
}

#[derive(Debug, Deserialize, Serialize)]
pub enum Expression {
    Literal(Literal),
    BinaryOp {
        op: BinaryOperator,
        left: Box<Expression>,
        right: Box<Expression>,
    },
}

#[derive(Debug, Deserialize, Serialize)]
pub enum Literal {
    Integer(i64),
    String(String),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum BinaryOperator {
    Add,
    Sub,
    Mul,
    Div,
}
```

**Día 5-7: Validator**

```rust
// src/validator/mod.rs

use crate::parser::template::*;
use anyhow::{Result, bail};

pub struct Validator;

impl Validator {
    pub fn validate_template(template: &Template) -> Result<()> {
        match template {
            Template::Program(prog) => Self::validate_program(prog),
            Template::Function(func) => Self::validate_function(func),
        }
    }

    fn validate_program(prog: &ProgramTemplate) -> Result<()> {
        // Name must not be empty
        if prog.name.is_empty() {
            bail!("Program name cannot be empty");
        }

        // Must have at least one action
        if prog.actions.is_empty() {
            bail!("Program must have at least one action");
        }

        // Validate each action
        for action in &prog.actions {
            Self::validate_action(action)?;
        }

        Ok(())
    }

    fn validate_action(action: &Action) -> Result<()> {
        match action {
            Action::Print(msg) => {
                if msg.is_empty() {
                    bail!("Print message cannot be empty");
                }
                Ok(())
            }
        }
    }

    fn validate_function(func: &FunctionTemplate) -> Result<()> {
        // Name validation
        if func.name.is_empty() {
            bail!("Function name cannot be empty");
        }

        // Implementation validation
        if func.implementation.is_empty() {
            bail!("Function must have implementation");
        }

        Ok(())
    }
}
```

### Semana 2: Code Generator + Integration

**Día 8-10: Assembly Code Generator**

```rust
// src/codegen/asm_x64.rs

use crate::parser::template::*;
use anyhow::Result;

pub struct X64AsmGenerator {
    output: String,
}

impl X64AsmGenerator {
    pub fn new() -> Self {
        Self {
            output: String::new(),
        }
    }

    pub fn generate(&mut self, template: &Template) -> Result<String> {
        match template {
            Template::Program(prog) => self.generate_program(prog),
            Template::Function(func) => self.generate_function(func),
        }
    }

    fn generate_program(&mut self, prog: &ProgramTemplate) -> Result<String> {
        // AT&T syntax x86-64 assembly
        self.emit(".global main");
        self.emit(".type main, @function");
        self.emit("");
        self.emit("main:");
        self.emit("    pushq %rbp");
        self.emit("    movq %rsp, %rbp");
        self.emit("");

        // Generate code for each action
        for action in &prog.actions {
            self.generate_action(action)?;
        }

        // Exit with code 0
        self.emit("");
        self.emit("    # Exit");
        self.emit("    movq $60, %rax");  // syscall: exit
        self.emit("    xorq %rdi, %rdi"); // status: 0
        self.emit("    syscall");

        Ok(self.output.clone())
    }

    fn generate_action(&mut self, action: &Action) -> Result<()> {
        match action {
            Action::Print(msg) => {
                let label = format!("str_{}", self.gen_unique_id());

                // Write syscall
                self.emit("    # Print");
                self.emit("    movq $1, %rax");           // syscall: write
                self.emit("    movq $1, %rdi");           // fd: stdout
                self.emit(&format!("    leaq {}(%rip), %rsi", label));
                self.emit(&format!("    movq ${}, %rdx", msg.len()));
                self.emit("    syscall");
                self.emit("");

                // String data (in .data section - will add later)
                self.data_section.push(format!("{}:", label));
                self.data_section.push(format!("    .ascii {:?}", msg));
            }
        }
        Ok(())
    }

    fn generate_function(&mut self, func: &FunctionTemplate) -> Result<String> {
        // Function prologue
        self.emit(&format!(".global {}", func.name));
        self.emit(&format!(".type {}, @function", func.name));
        self.emit("");
        self.emit(&format!("{}:", func.name));
        self.emit("    pushq %rbp");
        self.emit("    movq %rsp, %rbp");
        self.emit("");

        // Function body
        for stmt in &func.implementation {
            self.generate_statement(stmt)?;
        }

        // Function epilogue
        self.emit("    popq %rbp");
        self.emit("    ret");

        Ok(self.output.clone())
    }

    fn emit(&mut self, line: &str) {
        self.output.push_str(line);
        self.output.push('\n');
    }

    fn gen_unique_id(&self) -> usize {
        // Simple counter, can be improved
        use std::sync::atomic::{AtomicUsize, Ordering};
        static COUNTER: AtomicUsize = AtomicUsize::new(0);
        COUNTER.fetch_add(1, Ordering::SeqCst)
    }
}
```

**Día 11-12: CLI y Build Pipeline**

```rust
// src/main.rs

use anyhow::{Result, Context};
use std::fs;
use std::path::PathBuf;
use std::process::Command;

mod parser;
mod validator;
mod codegen;

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 {
        eprintln!("Usage: chronos <input.chronos>");
        std::process::exit(1);
    }

    let input_path = PathBuf::from(&args[1]);
    let output_name = input_path.file_stem().unwrap().to_str().unwrap();

    // 1. Read template
    println!("Reading template: {}", input_path.display());
    let template_src = fs::read_to_string(&input_path)
        .context("Failed to read template file")?;

    // 2. Parse YAML
    println!("Parsing template...");
    let template: parser::template::Template = serde_yaml::from_str(&template_src)
        .context("Failed to parse template")?;

    // 3. Validate
    println!("Validating template...");
    validator::Validator::validate_template(&template)
        .context("Template validation failed")?;

    // 4. Generate assembly
    println!("Generating assembly...");
    let mut codegen = codegen::asm_x64::X64AsmGenerator::new();
    let asm_code = codegen.generate(&template)
        .context("Code generation failed")?;

    // 5. Write assembly file
    let asm_path = format!("{}.s", output_name);
    fs::write(&asm_path, &asm_code)
        .context("Failed to write assembly file")?;
    println!("Generated: {}", asm_path);

    // 6. Assemble (using GNU as)
    println!("Assembling...");
    let obj_path = format!("{}.o", output_name);
    let status = Command::new("as")
        .arg(&asm_path)
        .arg("-o")
        .arg(&obj_path)
        .status()
        .context("Failed to run assembler")?;

    if !status.success() {
        anyhow::bail!("Assembly failed");
    }

    // 7. Link (using ld)
    println!("Linking...");
    let exe_path = output_name;
    let status = Command::new("ld")
        .arg(&obj_path)
        .arg("-o")
        .arg(&exe_path)
        .status()
        .context("Failed to run linker")?;

    if !status.success() {
        anyhow::bail!("Linking failed");
    }

    println!("✓ Build successful: {}", exe_path);
    println!();
    println!("Run with: ./{}", exe_path);

    Ok(())
}
```

**Día 13-14: Testing y Polish**

```rust
// tests/integration_tests.rs

use std::process::Command;
use std::fs;

#[test]
fn test_hello_world() {
    // Create template
    let template = r#"
Program:
  name: hello
  description: Hello world program
  actions:
    - Print: "Hello, World!\n"
"#;

    fs::write("test_hello.chronos", template).unwrap();

    // Compile
    let output = Command::new("cargo")
        .args(&["run", "--", "test_hello.chronos"])
        .output()
        .unwrap();

    assert!(output.status.success());

    // Run executable
    let output = Command::new("./hello")
        .output()
        .unwrap();

    assert!(output.status.success());
    assert_eq!(String::from_utf8_lossy(&output.stdout), "Hello, World!\n");

    // Cleanup
    fs::remove_file("test_hello.chronos").ok();
    fs::remove_file("hello").ok();
    fs::remove_file("hello.s").ok();
    fs::remove_file("hello.o").ok();
}

#[test]
fn test_simple_function() {
    let template = r#"
Function:
  name: add
  description: Add two numbers
  inputs:
    - name: a
      type: I32
    - name: b
      type: I32
  output: I32
  implementation:
    - Return:
        BinaryOp:
          op: Add
          left:
            Literal:
              Integer: 0  # placeholder for parameter 'a'
          right:
            Literal:
              Integer: 0  # placeholder for parameter 'b'
"#;

    // Test parsing and validation
    let template: parser::template::Template = serde_yaml::from_str(template).unwrap();
    validator::Validator::validate_template(&template).unwrap();
}
```

---

## Milestone 1: Entregables

Al final de las 2 semanas tendremos:

✅ **Compiler funcional** (chronos-compiler)
```bash
$ cargo build --release
$ ./target/release/chronos examples/hello.chronos
Reading template: examples/hello.chronos
Parsing template...
Validating template...
Generating assembly...
Generated: hello.s
Assembling...
Linking...
✓ Build successful: hello

Run with: ./hello
```

✅ **Ejemplo funcional**
```bash
$ ./hello
Hello, World!
```

✅ **Tests**
```bash
$ cargo test
running 2 tests
test test_hello_world ... ok
test test_simple_function ... ok
```

✅ **Documentación**
- README.md del compiler
- Ejemplos en examples/
- Tests como documentación

---

## Milestone 2: Próximos Pasos (Semanas 3-4)

Una vez que Milestone 1 funcione:

**Agregar más templates:**
- ✅ Function (con parámetros reales)
- ✅ Variable declarations
- ✅ Control flow (if, while)
- ✅ Basic arithmetic

**Mejorar codegen:**
- ✅ Register allocation
- ✅ Stack management
- ✅ Function calls

**Integrar assembler/linker propio:**
- ✅ No depender de GNU as/ld
- ✅ Generar ELF64 directo

---

## Archivo de Ejemplo

**examples/hello.chronos:**
```yaml
Program:
  name: hello
  description: Simple hello world program

  actions:
    - Print: "Hello, World!\n"
```

**examples/fibonacci.chronos:**
```yaml
Function:
  name: fib
  description: Calculate fibonacci number

  inputs:
    - name: n
      type: I32

  output: I32

  implementation:
    - If:
        condition:
          BinaryOp:
            op: LessOrEqual
            left:
              Variable: n
            right:
              Literal:
                Integer: 1
        then:
          - Return:
              Variable: n
        else:
          - Return:
              BinaryOp:
                op: Add
                left:
                  Call:
                    function: fib
                    arguments:
                      - BinaryOp:
                          op: Sub
                          left:
                            Variable: n
                          right:
                            Literal:
                              Integer: 1
                right:
                  Call:
                    function: fib
                    arguments:
                      - BinaryOp:
                          op: Sub
                          left:
                            Variable: n
                          right:
                            Literal:
                              Integer: 2
```

---

## Siguiente Sesión

**Tareas inmediatas:**
1. Setup del proyecto Rust
2. Implementar parser básico
3. Implementar validator básico
4. Implementar codegen para "Hello World"
5. Test end-to-end

**¿Comenzamos con el setup?**

```bash
# Esto es lo que ejecutaremos
mkdir chronos-compiler
cd chronos-compiler
cargo init
# ... y seguimos con la implementación
```

---

**¿Listo para escribir código?** 🚀
