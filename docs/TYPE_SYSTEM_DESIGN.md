# Chronos Type System Design v1.0

**Date:** October 29, 2025
**Status:** Design Complete - Implementation Pending
**Target:** v1.0 Release

---

## Overview

The Chronos type system is designed to be **deterministic, safe, and explicit**. No undefined behavior, no implicit coercions, no surprises.

### Core Principles

1. **Strong Static Typing** - All types checked at compile-time
2. **No Implicit Conversions** - Explicit casts required
3. **Deterministic Behavior** - Same code always produces same result
4. **Memory Safety** - Ownership prevents use-after-free
5. **Zero-Cost Abstractions** - Type system has no runtime overhead

---

## Type Hierarchy

```
Type
├── Primitive
│   ├── Void
│   ├── Bool
│   ├── Integer
│   │   ├── Signed (i8, i16, i32, i64, i128)
│   │   └── Unsigned (u8, u16, u32, u64, u128)
│   └── Float (f32, f64)
├── Pointer (*T)
├── Array ([T; N])
├── Struct
└── Function (fn(T1, T2) -> T3)
```

---

## Data Structures

### TypeInfo

Represents a single type in the system.

```chronos
struct TypeInfo {
    id: i64,              // Unique type ID
    name: [i8; 32],       // "i64", "bool", "*i64", etc.
    kind: i64,            // TYPE_PRIMITIVE, TYPE_POINTER, etc.
    size: i64,            // Size in bytes
    is_signed: i64,       // 1 = signed, 0 = unsigned (for integers)
    base_type_id: i64,    // For pointers/arrays: ID of base type
    alignment: i64        // Alignment requirement
}
```

**Fields:**
- `id`: Unique identifier (0-99 for built-ins, 100+ for custom)
- `name`: Human-readable name
- `kind`: Category of type (see Type Kinds below)
- `size`: Memory footprint in bytes
- `is_signed`: Only relevant for integer types
- `base_type_id`: For composite types (pointer to what? array of what?)
- `alignment`: Memory alignment requirement (usually equals size for primitives)

### TypeTable

Global registry of all types.

```chronos
struct TypeTable {
    types: [TypeInfo; 128],    // Max 128 types
    count: i64,                // Current number of types
    next_id: i64               // Next ID for custom types
}
```

**Operations:**
- `typetable_init()` - Initialize with primitive types
- `typetable_lookup_by_name()` - Find type by name string
- `typetable_lookup_by_id()` - Find type by ID
- `typetable_register_pointer()` - Create pointer type
- `typetable_register_struct()` - Create struct type
- `typetable_dump()` - Debug print all types

---

## Type Kinds

```chronos
let TYPE_PRIMITIVE: i64 = 0;    // Built-in types
let TYPE_POINTER: i64 = 1;      // *T
let TYPE_ARRAY: i64 = 2;        // [T; N]
let TYPE_STRUCT: i64 = 3;       // struct { ... }
let TYPE_FUNCTION: i64 = 4;     // fn(...) -> T
```

---

## Primitive Types

### Integer Types

| Type | ID | Size | Signed | Min Value | Max Value |
|------|----|------|--------|-----------|-----------|
| i8   | 2  | 1    | Yes    | -128      | 127       |
| i16  | 3  | 2    | Yes    | -32768    | 32767     |
| i32  | 4  | 4    | Yes    | -2^31     | 2^31-1    |
| i64  | 5  | 8    | Yes    | -2^63     | 2^63-1    |
| u8   | 6  | 1    | No     | 0         | 255       |
| u16  | 7  | 2    | No     | 0         | 65535     |
| u32  | 8  | 4    | No     | 0         | 2^32-1    |
| u64  | 9  | 8    | No     | 0         | 2^64-1    |

### Other Primitives

| Type | ID | Size | Description |
|------|----|------|-------------|
| void | 0  | 0    | Unit type (no value) |
| bool | 1  | 1    | Boolean (0 or 1) |
| f32  | 10 | 4    | IEEE 754 single precision |
| f64  | 11 | 8    | IEEE 754 double precision |

---

## Type Checking Rules

### Rule 1: Exact Type Matching

**No implicit conversions**

```chronos
fn foo() -> i64 { return 42; }

fn main() -> i32 {
    return foo();  // ❌ ERROR: Type mismatch
                   //    Expected: i32
                   //    Got: i64
                   //    Help: Use explicit cast: return foo() as i32;
}
```

**Implementation:**
```chronos
fn typecheck_compatible(table: *TypeTable, type1: i64, type2: i64) -> i64 {
    // Types must be exactly equal
    return type1 == type2;
}
```

### Rule 2: Explicit Casts Only

**All type conversions must be explicit**

```chronos
let x: i32 = 42;
let y: i64 = x;         // ❌ ERROR: No implicit conversion
let z: i64 = x as i64;  // ✅ OK: Explicit cast
```

**Valid casts:**
- Integer → Integer (any size/signedness)
- Float → Float
- Integer → Float
- Float → Integer (truncates)
- Integer → Pointer (unsafe!)
- Pointer → Integer
- Pointer → Pointer

**Invalid casts:**
- Primitive → Struct
- Struct → Primitive
- Function → anything
- Array → anything (except pointer to first element)

### Rule 3: Pointer Safety

**Pointers must be explicitly typed**

```chronos
let ptr: *i64 = malloc(8);  // ❌ ERROR: malloc returns *void
let ptr: *i64 = malloc(8) as *i64;  // ✅ OK: Explicit cast
```

### Rule 4: No Null Pointer Arithmetic

**Pointer arithmetic must be safe**

```chronos
let ptr: *i64 = 0 as *i64;  // NULL pointer
let value: i64 = ptr[0];    // ❌ PANIC: Null pointer dereference
```

---

## Type Checking Process

### Phase 1: Type Registration

During parsing, register all types:

```chronos
// Parse: struct Point { x: i64, y: i64 }
typetable_register_struct(table, "Point", 16);

// Parse: fn foo() -> *i64
let ptr_i64: i64 = typetable_register_pointer(table, TYPE_I64);
```

### Phase 2: Expression Type Checking

Check each expression has valid type:

```chronos
fn typecheck_expr(expr: *Expr, table: *TypeTable) -> i64 {
    if (expr.type == EXPR_BINARY) {
        let left_type: i64 = typecheck_expr(expr.left, table);
        let right_type: i64 = typecheck_expr(expr.right, table);

        if (!typecheck_compatible(table, left_type, right_type)) {
            type_error_incompatible(table, expr.line, left_type, right_type);
            return TYPE_UNKNOWN;
        }

        return left_type;
    }
    // ... more cases
}
```

### Phase 3: Function Type Checking

Verify function signatures:

```chronos
fn typecheck_function(func: *Function, table: *TypeTable) -> i64 {
    // Check return type matches actual returns
    let declared_return: i64 = func.return_type;
    let actual_return: i64 = typecheck_expr(func.body, table);

    if (!typecheck_compatible(table, declared_return, actual_return)) {
        type_error_incompatible(table, func.line, declared_return, actual_return);
        return 0;
    }

    return 1;
}
```

---

## Error Messages

### Example 1: Type Mismatch

```
ERROR at line 10, column 12:
    return foo();
           ^^^
Type mismatch: expected i32, got i64

Help: Use explicit cast:
    return foo() as i32;
```

### Example 2: Invalid Cast

```
ERROR at line 15, column 20:
    let x: Point = 42 as Point;
                   ^^
Cannot cast i64 to Point

Note: Structs cannot be created from primitives
```

### Example 3: Null Pointer

```
PANIC at line 8:
Null pointer dereference detected

Stack trace:
    at main (test.ch:8)
```

---

## Type System Features (v1.0)

### ✅ Implemented in v1.0

- [x] Primitive types (i8-i64, u8-u64, bool, void)
- [x] Pointer types (*T)
- [x] Type table with lookup
- [x] Type checking for expressions
- [x] Type checking for functions
- [x] Explicit casting only
- [x] Clear error messages
- [x] Pointer safety checks

### 🚧 Planned for v1.1+

- [ ] Array types with size tracking
- [ ] Struct type checking
- [ ] Generic types (parametric polymorphism)
- [ ] Type inference (minimal)
- [ ] Trait/interface system
- [ ] Dependent types (arrays with compile-time size)

---

## Implementation Status

### Current Status (v0.17)

**Parser:** ✅ Recognizes type annotations
**Type Table:** ❌ Not implemented
**Type Checking:** ❌ Not implemented
**Error Messages:** ⚠️ Basic parsing errors only

### Sprint 1 Goal (Weeks 1-2)

**Type Table:** ✅ Designed, ready to implement
**Type Checking:** ⚠️ Design complete, needs compiler support
**Error Messages:** 📝 Designed

### Blocker

The current bootstrap compiler (chronos_v10) **doesn't support:**
- Structs with methods
- Arrays of structs
- Field access in structs
- Complex expressions

**Solution:** This is exactly why we need v1.0! The type checker will be implemented once we have a more capable compiler.

---

## Testing Strategy

### Unit Tests

```chronos
// Test 1: Primitive type lookup
let id: i64 = typetable_lookup_by_name(table, "i64");
assert(id == TYPE_I64);

// Test 2: Pointer type registration
let ptr_id: i64 = typetable_register_pointer(table, TYPE_I64);
assert(ptr_id != TYPE_UNKNOWN);

// Test 3: Type compatibility
assert(typecheck_compatible(table, TYPE_I64, TYPE_I64));
assert(!typecheck_compatible(table, TYPE_I64, TYPE_I32));

// Test 4: Valid cast
assert(typecheck_can_cast(table, TYPE_I32, TYPE_I64));

// Test 5: Invalid cast
assert(!typecheck_can_cast(table, TYPE_I32, TYPE_BOOL));
```

### Integration Tests

```chronos
// Test 6: Function type checking
fn foo() -> i64 { return 42; }

fn bar() -> i32 {
    return foo();  // Should error: i64 vs i32
}

// Test 7: Explicit cast
fn baz() -> i32 {
    return foo() as i32;  // Should succeed
}

// Test 8: Pointer safety
fn qux() -> i64 {
    let ptr: *i64 = 0 as *i64;
    return ptr[0];  // Should panic at runtime
}
```

---

## Performance Considerations

### Type Checking Overhead

**Compile-time only** - Zero runtime cost

- Type checking happens during compilation
- No type information in generated binary
- No runtime type checks (unless explicit)

### Type Table Size

```
128 types maximum in v1.0
Each TypeInfo: ~96 bytes
Total: ~12 KB maximum

Negligible memory usage
```

### Lookup Performance

```
Linear search: O(n)
Acceptable for n ≤ 128

Future: Hash table for O(1) lookup
```

---

## Comparison with Other Languages

### vs C

| Feature | C | Chronos |
|---------|---|---------|
| Implicit casts | ✅ Everywhere | ❌ Never |
| Type safety | ❌ Weak | ✅ Strong |
| Null checks | ❌ No | ✅ Yes (runtime) |
| Overflow | ❌ UB | ✅ Checked |

### vs Rust

| Feature | Rust | Chronos v1.0 |
|---------|------|--------------|
| Strong typing | ✅ | ✅ |
| Ownership | ✅ Full | ⚠️ Basic |
| Generics | ✅ | ❌ (v1.1) |
| Traits | ✅ | ❌ (v1.2) |
| Type inference | ✅ Full | ⚠️ Minimal |

### vs Go

| Feature | Go | Chronos |
|---------|-----|---------|
| Implicit casts | ⚠️ Some | ❌ None |
| Null safety | ❌ No | ✅ Yes |
| Generics | ✅ (new) | ❌ (v1.1) |
| Simple | ✅ | ✅ |

---

## Next Steps

### Immediate (Sprint 1)

1. **Expand compiler_main.ch** to support:
   - Struct definitions
   - Field access
   - Arrays of structs
   - More complex expressions

2. **Implement typechecker.ch** (once compiler supports it)

3. **Write comprehensive tests**

### Short-term (Sprint 2)

4. **Integrate with parser**
   - Parser calls type checker
   - Type errors reported

5. **Add type checking to all expressions**

6. **Implement checked arithmetic**

### Medium-term (Sprint 3-4)

7. **Add ownership system**

8. **Implement borrow checker**

9. **Complete v1.0 type system**

---

## Conclusion

The Chronos type system is designed to be:

✅ **Safe** - No undefined behavior
✅ **Explicit** - No surprises
✅ **Deterministic** - Predictable behavior
✅ **Fast** - Zero runtime overhead

**Current blocker:** Need better compiler to implement it

**Solution:** Build it incrementally as part of v1.0

---

**Author:** Chronos Type System Design Team
**Version:** 1.0.0-design
**Last Updated:** October 29, 2025
**Status:** Ready for implementation once compiler supports it
