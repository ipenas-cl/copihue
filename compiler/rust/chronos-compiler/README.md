# Chronos Compiler v0.0.1

Template-based deterministic programming language compiler.

## Status: Milestone 1 - Minimal Viable Compiler

**Goal:** Compile "Hello World" template to executable.

**Current:** Implementation in progress.

## Building

Requires Rust 1.70+ (install from https://rustup.rs)

```bash
cargo build --release
```

## Usage

```bash
# Compile a template
./target/release/chronos examples/hello.chronos

# Run the generated executable
./hello
```

### With verbose output

```bash
./target/release/chronos -v examples/hello.chronos
```

### Keep intermediate files

```bash
./target/release/chronos --keep examples/hello.chronos
# Generates: hello.s (assembly), hello.o (object), hello (executable)
```

## Template Format

Chronos programs are written in YAML-based templates.

### Program Template

Simplest form - a list of actions to execute:

```yaml
Program:
  name: hello
  description: Hello world program

  actions:
    - Print: "Hello, World!"
```

### Function Template (coming soon)

```yaml
Function:
  name: add
  description: Add two numbers

  inputs:
    - name: a
      type: i32
    - name: b
      type: i32

  output: i32

  implementation:
    - Return:
        value:
          BinaryOp:
            op: Add
            left:
              Variable: a
            right:
              Variable: b
```

## Compilation Pipeline

```
Template (.chronos)
    ↓
[YAML Parser]
    ↓
[Validator]
    ↓
[Code Generator]
    ↓
Assembly (.s)
    ↓
[GNU as]
    ↓
Object (.o)
    ↓
[GNU ld]
    ↓
Executable
```

## Supported Features (Milestone 1)

- ✅ Program template
- ✅ Print action
- ✅ x86-64 code generation (AT&T syntax)
- ✅ Linux syscalls (write, exit)

## Coming Next (Milestone 2)

- Function templates
- Variables
- Control flow (if, while)
- Arithmetic expressions
- Function calls

## Architecture

```
src/
├── main.rs          # CLI and build pipeline
├── parser.rs        # YAML → AST
├── validator.rs     # Template validation
└── codegen.rs       # AST → x86-64 assembly
```

## Testing

```bash
cargo test
```

## Examples

See `examples/` directory:
- `hello.chronos` - Hello world

## Requirements

- Rust 1.70+
- GNU binutils (as, ld)
- Linux x86-64

## License

MIT
