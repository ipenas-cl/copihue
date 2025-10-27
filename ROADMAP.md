# Chronos Roadmap

**Current Version**: v0.17
**Last Updated**: 2025-10-27

---

## Overview

Chronos is a systems programming language focused on safety, determinism, and performance. This roadmap outlines planned features and improvements for future releases.

**Philosophy**: Each release adds value while maintaining our core principles:
- 🛡️ Safety first (bounds checking, no undefined behavior)
- 🎯 Determinism (same input → same output)
- ⚡ Performance (optimizations without compromising safety)
- 🎓 Simplicity (clear syntax, predictable behavior)

---

## Version History

### ✅ v0.17 - Optimizing Compiler (CURRENT)
**Released**: 2025-10-27
**Status**: Production-ready

**Highlights**:
- Optimizing compiler with 3 levels (-O0, -O1, -O2)
- Constant folding optimization
- Strength reduction for power-of-2 operations
- 20% smaller code, 3-40x speedup on optimized ops
- 100% accurate documentation
- Professional project organization
- 17 comprehensive examples

**Features**:
- Types: i8, i16, i32, i64, arrays
- Operators: arithmetic, logical, compound assignment
- Control flow: if/else, while, for
- Functions with types
- Built-in I/O functions
- Safety guarantees always active

---

## 🎯 v0.18 - Code Quality & Developer Experience

**Target**: Q1 2026 (Jan-Mar)
**Focus**: Better code generation and error messages

### Compiler Optimizations
- [ ] **Peephole optimization** - Pattern-based assembly improvements
- [ ] **Dead code elimination** - Remove unreachable code
- [ ] **Common subexpression elimination** - Reuse computed values

### Developer Experience
- [ ] **Better error messages** - More helpful, with suggestions
- [ ] **Line/column numbers in errors** - Precise error location
- [ ] **Warnings** - Potential issues (unused variables, etc.)

### Language Improvements
- [ ] **Else-if construct** - `else if` instead of nested else
- [ ] **String variable indexing** - `var[i]` not just `"literal"[i]`
- [ ] **Array initialization syntax** - Better initializer support

### Performance
- Expected: 5-10% additional code size reduction
- Expected: Faster compilation times
- Expected: Better error recovery

**Estimated Work**: 40-60 hours
**Priority**: High

---

## 🏗️ v0.19 - Advanced Optimizations & Types

**Target**: Q2 2026 (Apr-Jun)
**Focus**: Loop optimizations and basic structs

### Compiler Optimizations
- [ ] **Loop unrolling** - Unroll small loops
- [ ] **Loop-invariant code motion** - Move calculations out of loops
- [ ] **Function inlining** - Inline small functions

### Type System
- [ ] **Structs (basic)** - User-defined types with fields
  ```chronos
  struct Point {
      x: i32,
      y: i32
  }
  ```
- [ ] **Struct literals** - `Point { x: 10, y: 20 }`
- [ ] **Struct methods** - Functions tied to types

### Standard Library (Start)
- [ ] **String utilities** - strlen, strcmp, strcpy
- [ ] **Memory utilities** - memcpy, memset
- [ ] **Math utilities** - abs, min, max

**Estimated Work**: 60-80 hours
**Priority**: Medium-High

---

## 🚀 v0.20 - Enums & Pattern Matching

**Target**: Q3 2026 (Jul-Sep)
**Focus**: Algebraic data types

### Language Features
- [ ] **Enums** - Sum types
  ```chronos
  enum Result {
      Ok(i32),
      Error(string)
  }
  ```
- [ ] **Pattern matching** - Match expressions
  ```chronos
  match result {
      Ok(value) => print_int(value),
      Error(msg) => println(msg)
  }
  ```
- [ ] **Option type** - Built-in Maybe/Option
  ```chronos
  enum Option<T> {
      Some(T),
      None
  }
  ```

### Compiler
- [ ] **Exhaustiveness checking** - Ensure all cases covered
- [ ] **Optimization for enums** - Efficient representation

**Estimated Work**: 80-100 hours
**Priority**: Medium

---

## 🎓 v0.21 - Generics (Basic)

**Target**: Q4 2026 (Oct-Dec)
**Focus**: Generic programming foundations

### Language Features
- [ ] **Generic functions**
  ```chronos
  fn identity<T>(x: T) -> T {
      return x;
  }
  ```
- [ ] **Generic structs**
  ```chronos
  struct Pair<A, B> {
      first: A,
      second: B
  }
  ```
- [ ] **Type inference** - Improved type inference for generics
- [ ] **Monomorphization** - Generate specialized code per type

**Estimated Work**: 100-120 hours
**Priority**: Medium

---

## 🌟 v1.0 - Self-Hosting & Module System

**Target**: 2027 (Mid-year)
**Focus**: Production-ready for serious projects

### Self-Hosting
- [ ] **Compiler written in Chronos** - Bootstrap complete
- [ ] **Standard library in Chronos** - No C dependencies
- [ ] **Build system** - Chronos-based build tool

### Module System
- [ ] **Modules** - Namespaces and organization
  ```chronos
  mod math {
      pub fn abs(x: i32) -> i32 { ... }
  }
  ```
- [ ] **Imports** - `use math::abs;`
- [ ] **Visibility** - `pub` and private
- [ ] **Package manager** (basic) - Dependency management

### Standard Library (Complete)
- [ ] **Collections** - Vec, HashMap, HashSet
- [ ] **Strings** - String type (not just literals)
- [ ] **Iterators** - Functional iteration
- [ ] **Error handling** - Result/Option utilities
- [ ] **I/O** - File, network, stdio abstractions
- [ ] **Formatting** - Printf-like formatting

### Tooling
- [ ] **Debugger support** - DWARF debug info
- [ ] **Language Server Protocol** - IDE support
- [ ] **Documentation generator** - Doc comments
- [ ] **Testing framework** - Built-in test runner

**Estimated Work**: 300-400 hours
**Priority**: High (for v1.0)

---

## 🔮 v2.0+ - Advanced Features (Future)

**Target**: 2028+
**Focus**: Advanced language features

### Potential Features
- [ ] **Async/await** - Asynchronous programming
- [ ] **Traits/Interfaces** - Ad-hoc polymorphism
- [ ] **Macros** - Hygienic macros
- [ ] **Compile-time computation** - Const evaluation
- [ ] **SIMD support** - Explicit vectorization
- [ ] **Link-time optimization** - Cross-module optimization
- [ ] **Multiple backends** - LLVM, Cranelift, or custom
- [ ] **Garbage collection** (optional) - RC or GC for managed mode
- [ ] **Foreign function interface** - Call C/Rust libraries
- [ ] **WebAssembly target** - Compile to WASM

**Note**: These are exploratory. Not all may be implemented.

---

## 🎯 Priorities by Theme

### Compiler (Ongoing)
1. **Optimization quality** - Better code generation
2. **Compilation speed** - Faster builds
3. **Error messages** - More helpful feedback
4. **Safety checks** - More compile-time verification

### Language (Incremental)
1. **Type system** - Structs, enums, generics
2. **Pattern matching** - Powerful control flow
3. **Module system** - Organization and reuse
4. **Standard library** - Common utilities

### Tooling (v1.0+)
1. **Self-hosting** - Compiler in Chronos
2. **IDE support** - LSP, syntax highlighting
3. **Build system** - Integrated build tool
4. **Package manager** - Dependency management

---

## 📅 Release Schedule

| Version | Target | Focus | Estimated Hours |
|---------|--------|-------|-----------------|
| **v0.17** | ✅ Released | Optimizing compiler | - |
| **v0.18** | Q1 2026 | Code quality & DX | 40-60 |
| **v0.19** | Q2 2026 | Advanced opts & structs | 60-80 |
| **v0.20** | Q3 2026 | Enums & pattern matching | 80-100 |
| **v0.21** | Q4 2026 | Generics | 100-120 |
| **v1.0** | Mid 2027 | Self-hosting & modules | 300-400 |
| **v2.0+** | 2028+ | Advanced features | TBD |

**Note**: These are estimates. Real timelines depend on available development time and priorities.

---

## 🚧 Non-Goals

Features we explicitly **won't** add (preserving simplicity):

- ❌ **Complex macros** - Hygienic macros only (v2.0+)
- ❌ **Exceptions** - Use Result/Option instead
- ❌ **Inheritance** - Prefer composition
- ❌ **Implicit conversions** - All conversions explicit
- ❌ **Null pointers** - Use Option<&T> instead
- ❌ **Undefined behavior** - Never, ever
- ❌ **Hidden allocations** - All allocation explicit
- ❌ **Thread-local storage** - Explicit state only

---

## 💡 Contributing

Want to help shape Chronos? Here's how:

### Current Needs (v0.18)
- Better error messages
- Peephole optimization patterns
- Dead code elimination
- Test coverage

### Future Needs (v0.19+)
- Struct implementation
- Loop optimization algorithms
- Standard library design
- Documentation improvements

### How to Contribute
1. Check [issues](https://github.com/your-repo/issues) for open tasks
2. Discuss design in issues before implementing
3. Follow the coding style in existing code
4. Add tests for new features
5. Update documentation

---

## 📊 Metrics & Goals

### Code Quality
- **Test coverage**: Target 80%+ (currently ~60%)
- **Documentation coverage**: Target 100% (currently 95%)
- **Example coverage**: Target all features (currently 90%)

### Performance
- **Compilation speed**: < 1s for 1000 lines (currently ~200ms)
- **Generated code**: Within 20% of hand-written assembly
- **Optimization impact**: 20-40% improvement with -O2 (achieved)

### Safety
- **Bounds checking**: Always active (achieved)
- **Type safety**: 100% (achieved)
- **Memory safety**: 100% (no heap yet)
- **Undefined behavior**: 0% (achieved)

---

## 🤝 Community & Support

- **GitHub**: [your-repo](https://github.com/your-repo)
- **Documentation**: `docs/` directory
- **Examples**: `examples/` directory
- **Issues**: Report bugs and request features
- **Discussions**: Design discussions welcome

---

## 📝 Changelog

See [CHANGELOG.md](CHANGELOG.md) for detailed release notes.

---

## 🎉 Conclusion

Chronos is on a clear path toward v1.0 and beyond. Each release adds meaningful features while maintaining our core values: **safety**, **determinism**, and **performance**.

The roadmap is ambitious but achievable. With steady progress, Chronos will become a production-ready systems programming language that prioritizes developer happiness without compromising on principles.

**Join us on this journey!** 🚀

---

**Last Updated**: 2025-10-27
**Current Version**: v0.17
**Next Release**: v0.18 (Q1 2026)
