# Chronos v1.0 - Roadmap Integrado

**Fecha:** 29 de Octubre, 2025
**Versión Actual:** v0.17
**Meta:** v1.0 - Lenguaje determinístico 100% self-hosted

---

## 🎯 Doble Objetivo de v1.0

### Objetivo 1: Determinismo Semántico ✅
- Type system robusto con checking
- Checked arithmetic por defecto
- Evaluation order especificado
- Ownership básico

### Objetivo 2: Self-Hosting Técnico ✅
- Eliminar dependencia de NASM
- Eliminar dependencia de LD
- Compilador 100% escrito en Chronos

**Estrategia:** Desarrollar ambos EN PARALELO para maximizar progreso.

---

## 📊 Timeline General (11 semanas)

```
Semana 1-2:   Type Checker + Assembler Foundation
Semana 3-4:   Checked Arithmetic + Assembler Core
Semana 5-6:   Linker + Evaluation Semantics
Semana 7-8:   Ownership Básico + Integration
Semana 9-10:  Testing + Refinement
Semana 11:    Bootstrap Final + v1.0 Release
```

---

## 🚀 Sprint Plan Detallado

### SPRINT 1: Fundamentos (Semanas 1-2)

**Objetivo:** Establecer bases para type checking y assembler

#### Track A: Type System
**Tareas:**
1. **Diseñar Type Table**
   ```chronos
   struct TypeInfo {
       name: [i8; 32],      // "i64", "bool", etc.
       size: i64,           // Bytes
       is_signed: i64,      // 1 = signed, 0 = unsigned
       is_pointer: i64,     // 1 = pointer type
       base_type: i64       // Para pointers: tipo base
   }

   struct TypeTable {
       types: [TypeInfo; 64],
       count: i64
   }
   ```

2. **Implementar Type Registration**
   - Registrar tipos primitivos (i8, i16, i32, i64, u8, u32, u64)
   - Registrar tipos compuestos (structs)
   - Registrar tipos pointer

3. **Parser → Type Table Integration**
   - Al parsear declaraciones, registrar en type table
   - Al parsear expresiones, anotar tipos

**Archivo:** `compiler/chronos/typechecker.ch` (nuevo, ~400 líneas)

**Tests:**
- Registrar tipo i64
- Registrar tipo pointer
- Lookup de tipos
- Error en tipo duplicado

---

#### Track B: Assembler Foundation
**Tareas:**
1. **Diseñar Instruction Table**
   ```chronos
   struct Instruction {
       mnemonic: [i8; 16],   // "mov", "add", etc.
       operand1: [i8; 32],   // "rax", "[rbp-8]", "42"
       operand2: [i8; 32],
       opcode: [u8; 15],     // Bytes de código máquina
       length: i64           // Longitud en bytes
   }

   struct InstructionTable {
       instructions: [Instruction; 1024],
       count: i64
   }
   ```

2. **Implementar Assembly Parser Básico**
   - Parsear líneas de assembly
   - Identificar mnemonic y operands
   - Categorizar operands (reg, imm, mem)

3. **Encoder para 5 instrucciones**
   ```asm
   mov rax, imm64    ; 48 B8 [imm64]
   mov rdi, rax      ; 48 89 C7
   syscall           ; 0F 05
   ret               ; C3
   push rbp          ; 55
   ```

**Archivo:** `compiler/chronos/assembler.ch` (nuevo, ~600 líneas)

**Tests:**
- Parsear "mov rax, 42"
- Encode mov rax, 42
- Encode syscall
- Generate 5-instruction program

---

**Deliverable Sprint 1:**
- ✅ Type table funcional
- ✅ Assembler básico (5 instrucciones)
- ✅ Tests pasando
- ✅ Documentación inicial

---

### SPRINT 2: Core Features (Semanas 3-4)

**Objetivo:** Type checking operacional + Assembler expandido

#### Track A: Type Checker Implementation
**Tareas:**
1. **Type Checking en Expresiones**
   ```chronos
   fn typecheck_expr(expr: *Expr, table: *TypeTable) -> i64 {
       if (expr.type == EXPR_BINARY) {
           let left_type: i64 = typecheck_expr(expr.left, table);
           let right_type: i64 = typecheck_expr(expr.right, table);

           // Verificar tipos compatibles
           if (left_type != right_type) {
               println("ERROR: Type mismatch");
               return -1;
           }

           return left_type;
       }
       // ... más casos
   }
   ```

2. **Type Checking en Funciones**
   - Verificar tipo de return vs declarado
   - Verificar tipos de parámetros en llamadas
   - Error si no coinciden

3. **Error Messages Claros**
   ```
   ERROR at line 10, column 5:
       return foo();
              ^^^
   Type mismatch: expected i32, got i64

   Help: Use explicit cast: return foo() as i32;
   ```

**Archivo:** Expandir `typechecker.ch` (+300 líneas)

**Tests:**
- Type check expresión aritmética
- Type check return statement
- Error en type mismatch
- Error message legible

---

#### Track B: Checked Arithmetic
**Tareas:**
1. **Implementar Overflow Detection**
   ```chronos
   fn checked_add_i64(a: i64, b: i64) -> i64 {
       let result: i64 = a + b;

       // Check overflow: si signos iguales y resultado diferente
       if (a > 0 && b > 0 && result < 0) {
           panic("Arithmetic overflow: addition");
       }
       if (a < 0 && b < 0 && result > 0) {
           panic("Arithmetic overflow: addition");
       }

       return result;
   }
   ```

2. **Code Generation con Checks**
   - Generar llamadas a checked_* en vez de add directo
   - Optimizar cuando compilador puede probar no overflow

3. **Implementar Variants**
   ```chronos
   // Standard (checked)
   let x: i64 = a + b;

   // Wrapping (explícito)
   let x: i64 = a.wrapping_add(b);

   // Saturating (explícito)
   let x: i64 = a.saturating_add(b);
   ```

**Archivo:** `compiler/chronos/checked_ops.ch` (nuevo, ~300 líneas)

**Tests:**
- Overflow detection i8
- Overflow detection i64
- Wrapping arithmetic
- Saturating arithmetic

---

#### Track C: Assembler Expansion
**Tareas:**
1. **Agregar 20+ instrucciones**
   ```asm
   ; Arithmetic
   add rax, rbx      ; 48 01 D8
   sub rax, rbx      ; 48 29 D8
   imul rax, rbx     ; 48 0F AF C3

   ; Stack
   pop rax           ; 58
   pop rbx           ; 5B

   ; Control flow
   jmp rel32         ; E9 [rel32]
   call rel32        ; E8 [rel32]

   ; Comparisons
   cmp rax, rbx      ; 48 39 D8
   test rax, rax     ; 48 85 C0

   ; More movs
   mov rbx, rax      ; 48 89 C3
   mov rcx, rax      ; 48 89 C1
   mov [rax], rbx    ; 48 89 18
   ```

2. **Implementar Symbol Table**
   ```chronos
   struct Symbol {
       name: [i8; 64],
       address: i64,
       section: i64,   // 0=.text, 1=.data, 2=.bss
       is_global: i64
   }

   fn add_symbol(table: *SymbolTable, name: *i8, addr: i64) -> i64;
   fn resolve_symbol(table: *SymbolTable, name: *i8) -> i64;
   ```

**Archivo:** Expandir `assembler.ch` (+400 líneas)

**Tests:**
- Encode 20+ instructions
- Symbol table add/resolve
- Generate multi-instruction program

---

**Deliverable Sprint 2:**
- ✅ Type checking funcional
- ✅ Checked arithmetic implementado
- ✅ Assembler con 25+ instrucciones
- ✅ Symbol table funcional
- ✅ 50+ tests pasando

---

### SPRINT 3: Linking & Semantics (Semanas 5-6)

**Objetivo:** Linker operacional + Evaluation semantics especificada

#### Track A: Evaluation Semantics
**Tareas:**
1. **Documentar Orden de Evaluación**
   - Crear `docs/EVALUATION_ORDER.md`
   - Especificar left-to-right
   - Ejemplos con side effects

2. **Implementar Short-Circuit Evaluation**
   ```chronos
   fn codegen_logical_and(left: *Expr, right: *Expr) -> i64 {
       // Evaluar lado izquierdo
       codegen_expr(left);

       // Si false, saltar evaluación de derecha
       println("    cmp rax, 0");
       println("    je .short_circuit_false");

       // Evaluar lado derecho
       codegen_expr(right);
       println("    jmp .short_circuit_end");

       println(".short_circuit_false:");
       println("    mov rax, 0");
       println(".short_circuit_end:");
   }
   ```

3. **Tests de Side Effects**
   ```chronos
   fn side_effect() -> i64 {
       println("Called!");
       return 1;
   }

   fn test_short_circuit() -> i64 {
       // Solo debe imprimir "Called!" una vez
       if (0 && side_effect()) {
           return 1;
       }
       return 0;
   }
   ```

**Archivo:** Expandir `compiler_main.ch` (+200 líneas)

**Tests:**
- Short-circuit &&
- Short-circuit ||
- Left-to-right evaluation
- Side effects ordenados

---

#### Track B: Linker Implementation
**Tareas:**
1. **ELF Object Reader**
   ```chronos
   struct ElfObject {
       header: ElfHeader,
       sections: [Section; 16],
       section_count: i64,
       symbols: [Symbol; 256],
       symbol_count: i64
   }

   fn read_elf_object(filename: *i8) -> *ElfObject;
   ```

2. **Symbol Resolution**
   ```chronos
   fn resolve_symbols(objects: *ElfObject, count: i64) -> i64 {
       // Construir symbol table global
       // Verificar no símbolos duplicados
       // Verificar todos los símbolos resueltos
       // Calcular direcciones finales
   }
   ```

3. **Relocation Application**
   ```chronos
   fn apply_relocations(obj: *ElfObject, base: i64) -> i64 {
       // Para cada relocation:
       //   1. Calcular dirección final
       //   2. Aplicar offset
       //   3. Patchear código/data
   }
   ```

4. **Executable Generation**
   ```chronos
   fn generate_executable(
       objects: *ElfObject,
       count: i64,
       output: *i8
   ) -> i64 {
       // Crear ELF header
       // Crear program headers
       // Escribir .text section
       // Escribir .data section
       // Configurar entry point
       // Escribir archivo
   }
   ```

**Archivo:** `compiler/chronos/linker.ch` (nuevo, ~800 líneas)

**Tests:**
- Read simple ELF object
- Resolve 2 symbols
- Apply relocation
- Generate executable

---

**Deliverable Sprint 3:**
- ✅ Evaluation order documentado
- ✅ Short-circuit evaluation funcional
- ✅ Linker básico operacional
- ✅ Pipeline: .o → executable
- ✅ 70+ tests pasando

---

### SPRINT 4: Ownership & Integration (Semanas 7-8)

**Objetivo:** Ownership básico + Integración completa

#### Track A: Ownership System
**Tareas:**
1. **Move Semantics**
   ```chronos
   fn foo() -> i64 {
       let x: *i64 = malloc(8) as *i64;
       x[0] = 42;

       let y: *i64 = x;  // x moved to y
       // x[0] = 10;     // ERROR: use after move

       free(y);
       return 42;
   }
   ```

2. **Borrow Checker Básico**
   ```chronos
   fn foo(ptr: *i64) -> i64 {
       return ptr[0];  // OK: borrow inmutable
   }

   fn bar(ptr: *mut i64) -> i64 {
       ptr[0] = 42;    // OK: borrow mutable
       return 0;
   }

   fn baz() -> i64 {
       let x: *i64 = malloc(8) as *i64;
       foo(x);         // Borrow
       bar(x);         // ERROR: cannot borrow as mutable while borrowed
       free(x);
       return 0;
   }
   ```

3. **Lifetime Tracking Simple**
   ```chronos
   struct OwnershipTracker {
       variables: [Variable; 256],
       var_count: i64
   }

   struct Variable {
       name: [i8; 64],
       is_moved: i64,      // 1 if moved
       borrow_count: i64,  // Number of active borrows
       is_mut_borrow: i64  // 1 if mutably borrowed
   }

   fn check_ownership(tracker: *OwnershipTracker, var: *i8) -> i64;
   ```

**Archivo:** `compiler/chronos/ownership.ch` (nuevo, ~500 líneas)

**Tests:**
- Move semantics
- Use after move error
- Borrow checking
- Mutable vs immutable borrow

---

#### Track B: Complete Integration
**Tareas:**
1. **Pipeline Completo**
   ```bash
   # source.ch → executable (sin NASM ni LD)

   # Step 1: Compile
   ./compiler_main source.ch > source.asm

   # Step 2: Assemble (usando assembler.ch en vez de NASM!)
   ./assembler source.asm source.o

   # Step 3: Link (usando linker.ch en vez de LD!)
   ./linker source.o runtime.o -o program

   # Step 4: Run
   ./program
   ```

2. **Runtime Library**
   ```chronos
   // runtime/runtime.ch

   fn panic(msg: *i8) -> i64 {
       write(2, msg, strlen(msg));  // stderr
       write(2, "\n", 1);
       exit(1);
       return 0;
   }

   fn checked_add_i64(a: i64, b: i64) -> i64 {
       // Implementation from Sprint 2
   }

   // ... más funciones runtime
   ```

3. **Wrapper Scripts**
   ```bash
   #!/bin/bash
   # tools/chronos-compile

   SOURCE=$1
   OUTPUT=${2:-a.out}

   # Compile
   ./compiler_main $SOURCE > /tmp/temp.asm

   # Assemble
   ./assembler /tmp/temp.asm /tmp/temp.o

   # Link
   ./linker /tmp/temp.o runtime.o -o $OUTPUT

   echo "✅ Compiled: $OUTPUT"
   ```

**Archivos:**
- `runtime/runtime.ch` (nuevo, ~400 líneas)
- `tools/chronos-compile` (nuevo)

**Tests:**
- Compile simple program
- Link with runtime
- Execute successfully
- End-to-end pipeline

---

**Deliverable Sprint 4:**
- ✅ Ownership básico funcional
- ✅ Pipeline completo sin NASM/LD
- ✅ Runtime library
- ✅ Wrapper scripts
- ✅ 100+ tests pasando

---

### SPRINT 5: Testing & Refinement (Semanas 9-10)

**Objetivo:** Comprehensive testing + Bug fixing

#### Week 9: Comprehensive Testing
**Tareas:**
1. **Type System Tests**
   - 30 test cases positivos
   - 30 test cases negativos (errors)
   - Edge cases

2. **Arithmetic Tests**
   - Overflow en todos los tipos
   - Underflow
   - Wrapping correctness
   - Saturating correctness

3. **Assembler Tests**
   - Todas las instrucciones
   - Symbol resolution
   - Multi-file assembly

4. **Linker Tests**
   - Symbol resolution compleja
   - Relocations
   - Multiple objects
   - Large programs

5. **Ownership Tests**
   - Use after move
   - Double free
   - Borrow violations
   - Lifetime errors

6. **Integration Tests**
   - 20 programas completos
   - Fibonacci
   - Factorial
   - Binary search
   - Sorting
   - File I/O
   - etc.

**Archivo:** `tests/v1/` (nuevo directorio con 100+ tests)

---

#### Week 10: Bug Fixing & Polish
**Tareas:**
1. **Fix Bugs from Testing**
   - Priorizar por severidad
   - Fix all critical bugs
   - Document known limitations

2. **Performance Optimization**
   - Profile compilation time
   - Optimize hot paths
   - Reduce memory usage

3. **Error Message Improvement**
   - Review all error messages
   - Add suggestions (did you mean?)
   - Add colors to terminal output

4. **Documentation**
   - Complete CHRONOS_SPEC.md
   - Update all READMEs
   - Write migration guide from v0.17

**Deliverable:** All tests passing, no critical bugs

---

### SPRINT 6: Bootstrap & Release (Semana 11)

**Objetivo:** Final bootstrap + v1.0 release

#### Week 11: Final Bootstrap
**Tareas:**
1. **Self-Compile Test**
   ```bash
   # Compile compiler with itself
   ./compiler_main compiler/chronos/compiler_main.ch > compiler.asm
   ./assembler compiler.asm compiler.o
   ./linker compiler.o runtime.o -o compiler_v1

   # Test new compiler
   ./compiler_v1 test.ch > test.asm
   ./assembler test.asm test.o
   ./linker test.o runtime.o -o test_program
   ./test_program  # Should work!
   ```

2. **Compile All Components**
   - Compiler with itself
   - Assembler with compiler
   - Linker with compiler
   - Runtime with compiler
   - **Full cycle: Chronos compiles all of Chronos**

3. **Verification**
   - Binary comparison
   - Test all components
   - Run full test suite
   - Performance benchmarks

4. **Release Preparation**
   - Tag v1.0.0
   - Update CHANGELOG
   - Create release notes
   - Package binaries

5. **Celebration! 🎉**

**Deliverable:** Chronos v1.0 - 100% determinístico, 100% self-hosted

---

## 📊 Métricas de Éxito v1.0

### Determinismo
- [ ] Type checking al 100%
- [ ] Checked arithmetic por defecto
- [ ] No undefined behavior posible
- [ ] Ownership previene use-after-free
- [ ] Evaluation order especificado

### Self-Hosting
- [ ] Sin dependencia de NASM
- [ ] Sin dependencia de LD
- [ ] Sin dependencia de C (excepto bootstrap inicial)
- [ ] Compiler compila compiler
- [ ] Full cycle funcional

### Calidad
- [ ] 200+ tests pasando
- [ ] 0 bugs críticos
- [ ] Performance aceptable (<10% overhead)
- [ ] Documentación completa
- [ ] Error messages útiles

### Features
- [ ] 40+ instrucciones en assembler
- [ ] Linking de múltiples objetos
- [ ] Ownership básico funcional
- [ ] Short-circuit evaluation
- [ ] Runtime library completa

---

## 📈 Estimaciones Detalladas

### Lines of Code
| Component | LOC | Status |
|-----------|-----|--------|
| typechecker.ch | 700 | 🆕 New |
| checked_ops.ch | 300 | 🆕 New |
| ownership.ch | 500 | 🆕 New |
| assembler.ch | 1200 | 🆕 New |
| linker.ch | 800 | 🆕 New |
| runtime.ch | 400 | 🆕 New |
| compiler_main.ch | +400 | ✏️ Update |
| **Total New Code** | **~4300 LOC** | |

### Time Estimates
| Phase | Duration | Effort (hours) |
|-------|----------|----------------|
| Sprint 1 | 2 weeks | 60-80 |
| Sprint 2 | 2 weeks | 80-100 |
| Sprint 3 | 2 weeks | 80-100 |
| Sprint 4 | 2 weeks | 60-80 |
| Sprint 5 | 2 weeks | 80-100 |
| Sprint 6 | 1 week | 40-50 |
| **Total** | **11 weeks** | **~440 hours** |

---

## 🎯 Prioridades por Semana

### Weeks 1-2: ⚠️ CRÍTICO
- Type checker foundation
- Assembler foundation
- **Blocker for:** Everything else

### Weeks 3-4: ⚠️ CRÍTICO
- Type checking operational
- Checked arithmetic
- Assembler expanded
- **Blocker for:** Ownership, linking

### Weeks 5-6: 🔴 HIGH
- Linker implementation
- Evaluation semantics
- **Blocker for:** Full pipeline

### Weeks 7-8: 🟠 MEDIUM
- Ownership system
- Integration
- **Blocker for:** v1.0 quality

### Weeks 9-10: 🟡 LOW
- Testing
- Polish
- **Blocker for:** Release

### Week 11: 🎯 RELEASE
- Bootstrap
- Release

---

## 🚧 Risks & Mitigation

### Risk 1: Type Checker Complexity
**Probability:** Medium
**Impact:** High
**Mitigation:**
- Start with simple rules
- Expand gradually
- Extensive testing from day 1

### Risk 2: Linker Bugs
**Probability:** High
**Impact:** Critical
**Mitigation:**
- Test with simple programs first
- Compare output with LD
- Hexdump comparison

### Risk 3: Ownership Too Complex
**Probability:** Medium
**Impact:** Medium
**Mitigation:**
- Implement minimal viable version
- Can be expanded in v1.1
- Document limitations

### Risk 4: Timeline Slips
**Probability:** High
**Impact:** Low
**Mitigation:**
- Buffer time built in
- Can cut ownership if needed
- Incremental releases possible

---

## 📝 Architecture Decisions

### AD-1: Type System Approach
**Decision:** Strong static typing with minimal inference
**Rationale:**
- Easier to implement
- More predictable
- Clear error messages
- Can add inference later

### AD-2: Ownership Model
**Decision:** Rust-inspired but simpler
**Rationale:**
- Proven model
- Good documentation
- Start simple, expand later

### AD-3: Assembler Strategy
**Decision:** Direct x86-64 encoding, no intermediate
**Rationale:**
- Simpler implementation
- Fewer bugs
- Good performance
- Can optimize later

### AD-4: Linker Strategy
**Decision:** Static linking only (v1.0)
**Rationale:**
- Simpler than dynamic
- Self-contained binaries
- Dynamic linking in v1.1

---

## 🔄 Weekly Sync Points

Every Monday:
- Review progress
- Adjust priorities
- Identify blockers
- Update timeline

Deliverables each week:
- Code pushed
- Tests passing
- Documentation updated
- Demo ready

---

## 🎉 Success Criteria

### v1.0 is DONE when:
1. ✅ Compiler compiles itself
2. ✅ No external dependencies (NASM/LD)
3. ✅ Type checking prevents errors
4. ✅ Checked arithmetic by default
5. ✅ Ownership prevents use-after-free
6. ✅ 200+ tests passing
7. ✅ Documentation complete
8. ✅ Can compile non-trivial programs (1000+ LOC)

---

## 🚀 Post-v1.0 Roadmap (v1.1, v1.2, ...)

Ideas for future versions:
- v1.1: Dynamic linking
- v1.1: Generics
- v1.2: Macros
- v1.2: Module system
- v1.3: Concurrency primitives
- v1.3: Effect types
- v1.4: LLVM backend (optional)
- v2.0: Self-hosting bootstrap (compile bootstrap with Chronos)

---

**Última actualización:** 29 de Octubre, 2025
**Status:** ✅ PLAN COMPLETO
**Próximo paso:** Comenzar Sprint 1 - Type Checker Foundation

**¡VAMOS! 🔥**
