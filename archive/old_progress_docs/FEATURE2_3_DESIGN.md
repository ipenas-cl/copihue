# Feature 2+3 Design: Field Access (READ/WRITE)

**Date:** October 29, 2025
**Target:** Chronos v0.19
**Prerequisites:** Feature 1 (Struct Definition Parsing) ✅

---

## Goal

Enable field access for struct variables:

```chronos
struct Point{x:i64,y:i64}

fn main() -> i64 {
    let p: Point;    // Variable declaration
    p.x = 10;        // Field WRITE
    p.y = 32;        // Field WRITE
    return p.x;      // Field READ
}
```

**Expected output:** 42

---

## Incremental Implementation Strategy

### Phase 1: Ultra-Minimal (v0.19-alpha)
**Goal:** Get SOMETHING working, even if hardcoded

**Limitations:**
- Only ONE variable named "p"
- Variable type = first struct found
- Only supports: `p.field = NUMBER;`
- Only supports: `return p.field;`

**Code changes:** ~100 lines

### Phase 2: Basic Variables (v0.19-beta)
**Goal:** Support proper variable declarations

**Additions:**
- Parse: `let varname: StructType;`
- Variable table (up to 8 variables)
- Type lookup

**Code changes:** ~150 lines

### Phase 3: Full Expression Support (v0.19)
**Goal:** Field access in expressions

**Additions:**
- Parse: `p.x + p.y`
- Field access as operands
- Multiple field accesses

**Code changes:** ~150 lines

**Total:** ~400 lines over 3 phases

---

## Phase 1 Design: Ultra-Minimal

### Test Program

```chronos
struct Point{x:i64,y:i64}

fn main() -> i64 {
    p.x = 42;
    return p.x;
}
```

### Assumptions

1. Variable "p" exists implicitly
2. Type of "p" = first struct parsed
3. "p" is allocated on stack at `[rbp-SIZE]` where SIZE = struct size

### Parsing Strategy

**Step 1:** After struct parsing, before return parsing, scan for assignment statements

```chronos
// Scan for: p.field = NUMBER;
while (scanning function body) {
    if (found 'p' && next is '.') {
        // Parse field name
        // Parse '='
        // Parse number
        // Store: (field_name, value) pairs
    }
}
```

**Step 2:** Parse return statement with field access

```chronos
// Parse: return p.field;
if (found "return p.") {
    // Parse field name
    // Store: which field to return
}
```

### Code Generation Strategy

**Stack layout:**
```
[rbp-0]     <- return address
[rbp-8]     <- saved rbp
[rbp-16]    <- p.y (second field, offset 8)
[rbp-24]    <- p.x (first field, offset 0)
```

**For assignment: `p.x = 42;`**
```asm
mov rax, 42
mov [rbp-24], rax    ; rbp - struct_size + field_offset
                     ; = rbp - 16 + 0 = rbp - 16
```

**For assignment: `p.y = 10;`**
```asm
mov rax, 10
mov [rbp-16], rax    ; rbp - struct_size + field_offset
                     ; = rbp - 16 + 8 = rbp - 8
```

Wait, that's wrong. Let me recalculate:

If struct is 16 bytes and allocated from `[rbp-16]` to `[rbp-1]`:
- Field at offset 0 is at `[rbp-16]`
- Field at offset 8 is at `[rbp-8]`

So:
- `p.x` (offset 0) → `[rbp-16]`
- `p.y` (offset 8) → `[rbp-8]`

**For return: `return p.x;`**
```asm
mov rax, [rbp-16]    ; Load p.x
```

### Data Structures Needed

```chronos
// Assignment storage (max 16 assignments)
let g_assignments: [i64; 48];  // field_idx, value (2 i64s per assignment)
let g_assignment_count: i64 = 0;

// Return field (which field to return)
let g_return_field_idx: i64 = -1;  // -1 = return number, >= 0 = return field
```

### Functions to Add

1. **`parse_assignment_statements(source, start_pos, end_pos) -> i64`**
   - Scan function body for `p.field = NUMBER;`
   - Store in g_assignments

2. **`parse_return_with_field(source, pos) -> i64`**
   - Parse `return p.field;`
   - Store field index in g_return_field_idx

3. **`gen_struct_variable(cg, struct_idx) -> i64`**
   - Generate stack allocation: `sub rsp, SIZE`

4. **`gen_field_assignments(cg, struct_idx) -> i64`**
   - Generate mov instructions for each assignment

5. **`gen_field_return(cg, struct_idx, field_idx) -> i64`**
   - Generate: `mov rax, [rbp-offset]`

### Implementation Steps

1. Add data structures (lines 38-42)
2. Add helper functions (lines 600-700)
3. Modify main() to call new parsers (lines 810-850)
4. Modify gen_program() to generate new code (lines 276-320)

### Expected Output

```bash
$ ./chronos_program /tmp/test_minimal_field.ch

✅ Parsed 1 struct(s)
  struct Point { 2 fields, 16 bytes }
✅ Parsed 1 assignment(s)
  p.x = 42
✅ Parsed return: p.x
✅ Code generated

$ nasm -f elf64 output.asm && ld output.o -o program && ./program
$ echo $?
42
```

---

## Phase 2 Design: Basic Variables

### Test Program

```chronos
struct Point{x:i64,y:i64}

fn main() -> i64 {
    let p: Point;
    let q: Point;
    p.x = 10;
    q.x = 32;
    return p.x + q.x;
}
```

### Variable Table

```chronos
// Variable metadata (max 8 variables)
let g_var_names: [i8; 256];      // 8 * 32
let g_var_type_indices: [i64; 8];  // which struct type
let g_var_stack_offsets: [i64; 8]; // stack position
let g_var_count: i64 = 0;
```

### New Functions

1. **`parse_variable_declaration(source, pos) -> i64`**
   - Parse: `let varname: typename;`
   - Add to variable table
   - Calculate stack offset

2. **`lookup_variable(name) -> i64`**
   - Find variable in table
   - Return variable index

3. **`lookup_field_in_struct(struct_idx, field_name) -> i64`**
   - Find field in struct
   - Return field index

---

## Phase 3 Design: Full Expressions

### Test Program

```chronos
struct Point{x:i64,y:i64}

fn main() -> i64 {
    let p: Point;
    p.x = 10;
    p.y = 32;
    return p.x + p.y;  // Field access in expression
}
```

### Expression Parser Changes

Extend `parse_expression()` to handle:
- Numbers (already supported)
- Field access: `varname.fieldname`

### New Expression AST

```chronos
struct ExprNode {
    type: i64,     // 0=number, 1=field_access
    value: i64,    // number value OR field index
    var_idx: i64   // if field_access, which variable
}
```

---

## Risk Analysis

### Risk 1: Bootstrap Compiler Limitations

**Risk:** Bootstrap compiler might not support new syntax

**Mitigation:** Test each addition incrementally

**Fallback:** Simplify data structures (more flattening)

### Risk 2: Stack Offset Calculation

**Risk:** Incorrect offset calculations → memory corruption

**Mitigation:**
- Add debug output for all offsets
- Test with simple cases first
- Verify with GDB if needed

### Risk 3: Parser Complexity

**Risk:** Parser becomes too complex, hard to debug

**Mitigation:**
- Keep functions small (<100 lines)
- Add comments everywhere
- Test each parser function independently

---

## Testing Strategy

### Test 1: Single Field Write/Read
```chronos
p.x = 42;
return p.x;
```
Expected: 42

### Test 2: Multiple Fields
```chronos
p.x = 10;
p.y = 32;
return p.y;
```
Expected: 32

### Test 3: Field Arithmetic
```chronos
p.x = 10;
p.y = 32;
return p.x + p.y;
```
Expected: 42

### Test 4: Two Variables
```chronos
let p: Point;
let q: Point;
p.x = 20;
q.x = 22;
return p.x + q.x;
```
Expected: 42

---

## Timeline

### Phase 1 (Ultra-Minimal)
- **Time:** 2-3 hours
- **Output:** v0.19-alpha

### Phase 2 (Basic Variables)
- **Time:** 2-3 hours
- **Output:** v0.19-beta

### Phase 3 (Full Expressions)
- **Time:** 2-3 hours
- **Output:** v0.19

**Total:** 6-9 hours for complete Feature 2+3

---

## Next Actions

1. Start with Phase 1 implementation
2. Create `/tmp/test_minimal_field.ch`
3. Add minimal parsing functions
4. Test incrementally
5. Move to Phase 2 when Phase 1 works

---

## Success Criteria

Feature 2+3 complete when:
- ✅ Can declare struct variables
- ✅ Can write to fields
- ✅ Can read from fields
- ✅ Can use fields in expressions
- ✅ All tests pass
- ✅ No memory corruption
- ✅ Clean, documented code

---

**Ready to implement Phase 1** when user approves design.
