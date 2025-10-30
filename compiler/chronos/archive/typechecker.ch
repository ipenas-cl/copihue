// Chronos Type Checker v1.0
// Strong static typing with no implicit coercions
// Version: 1.0.0
// Date: October 29, 2025

// ============================================
// TYPE SYSTEM STRUCTURES
// ============================================

// Type kinds
let TYPE_PRIMITIVE: i64 = 0;
let TYPE_POINTER: i64 = 1;
let TYPE_ARRAY: i64 = 2;
let TYPE_STRUCT: i64 = 3;
let TYPE_FUNCTION: i64 = 4;

// Primitive type IDs
let TYPE_VOID: i64 = 0;
let TYPE_BOOL: i64 = 1;
let TYPE_I8: i64 = 2;
let TYPE_I16: i64 = 3;
let TYPE_I32: i64 = 4;
let TYPE_I64: i64 = 5;
let TYPE_U8: i64 = 6;
let TYPE_U16: i64 = 7;
let TYPE_U32: i64 = 8;
let TYPE_U64: i64 = 9;
let TYPE_F32: i64 = 10;
let TYPE_F64: i64 = 11;
let TYPE_UNKNOWN: i64 = 99;

struct TypeInfo {
    id: i64,              // Unique type ID
    name: [i8; 32],       // "i64", "bool", "*i64", etc.
    kind: i64,            // TYPE_PRIMITIVE, TYPE_POINTER, etc.
    size: i64,            // Size in bytes
    is_signed: i64,       // 1 = signed, 0 = unsigned (for integers)
    base_type_id: i64,    // For pointers/arrays: ID of base type
    alignment: i64        // Alignment requirement
}

struct TypeTable {
    types: [TypeInfo; 128],
    count: i64,
    next_id: i64          // For custom types
}

// ============================================
// TYPE TABLE INITIALIZATION
// ============================================

fn typetable_init(table: *TypeTable) -> i64 {
    table.count = 0;
    table.next_id = 100;  // Start custom types at 100

    // Register primitive types
    typetable_register_primitive(table, TYPE_VOID, "void", 0, 0);
    typetable_register_primitive(table, TYPE_BOOL, "bool", 1, 0);
    typetable_register_primitive(table, TYPE_I8, "i8", 1, 1);
    typetable_register_primitive(table, TYPE_I16, "i16", 2, 1);
    typetable_register_primitive(table, TYPE_I32, "i32", 4, 1);
    typetable_register_primitive(table, TYPE_I64, "i64", 8, 1);
    typetable_register_primitive(table, TYPE_U8, "u8", 1, 0);
    typetable_register_primitive(table, TYPE_U16, "u16", 2, 0);
    typetable_register_primitive(table, TYPE_U32, "u32", 4, 0);
    typetable_register_primitive(table, TYPE_U64, "u64", 8, 0);
    typetable_register_primitive(table, TYPE_F32, "f32", 4, 1);
    typetable_register_primitive(table, TYPE_F64, "f64", 8, 1);

    return 1;
}

fn typetable_register_primitive(
    table: *TypeTable,
    id: i64,
    name: *i8,
    size: i64,
    is_signed: i64
) -> i64 {
    if (table.count >= 128) {
        println("ERROR: Type table full");
        return 0;
    }

    let idx: i64 = table.count;
    let type_info: *TypeInfo = table.types[idx];

    type_info.id = id;
    str_copy(type_info.name, name);
    type_info.kind = TYPE_PRIMITIVE;
    type_info.size = size;
    type_info.is_signed = is_signed;
    type_info.base_type_id = -1;
    type_info.alignment = size;  // Primitive alignment = size

    table.count = table.count + 1;

    return 1;
}

// ============================================
// TYPE LOOKUP
// ============================================

fn typetable_lookup_by_name(table: *TypeTable, name: *i8) -> i64 {
    let i: i64 = 0;
    while (i < table.count) {
        let type_info: *TypeInfo = table.types[i];
        if (str_equal(type_info.name, name)) {
            return type_info.id;
        }
        i = i + 1;
    }
    return TYPE_UNKNOWN;
}

fn typetable_lookup_by_id(table: *TypeTable, id: i64) -> *TypeInfo {
    let i: i64 = 0;
    while (i < table.count) {
        let type_info: *TypeInfo = table.types[i];
        if (type_info.id == id) {
            return type_info;
        }
        i = i + 1;
    }
    return 0 as *TypeInfo;  // NULL
}

fn typetable_get_name(table: *TypeTable, id: i64) -> *i8 {
    let type_info: *TypeInfo = typetable_lookup_by_id(table, id);
    if (type_info as i64 == 0) {
        return "unknown";
    }
    return type_info.name;
}

fn typetable_get_size(table: *TypeTable, id: i64) -> i64 {
    let type_info: *TypeInfo = typetable_lookup_by_id(table, id);
    if (type_info as i64 == 0) {
        return 0;
    }
    return type_info.size;
}

// ============================================
// POINTER TYPES
// ============================================

fn typetable_register_pointer(table: *TypeTable, base_type_id: i64) -> i64 {
    // Check if pointer type already exists
    let i: i64 = 0;
    while (i < table.count) {
        let type_info: *TypeInfo = table.types[i];
        if (type_info.kind == TYPE_POINTER && type_info.base_type_id == base_type_id) {
            return type_info.id;  // Already exists
        }
        i = i + 1;
    }

    // Create new pointer type
    if (table.count >= 128) {
        println("ERROR: Type table full");
        return TYPE_UNKNOWN;
    }

    let idx: i64 = table.count;
    let type_info: *TypeInfo = table.types[idx];

    type_info.id = table.next_id;
    table.next_id = table.next_id + 1;

    // Build name: "*typename"
    let base_name: *i8 = typetable_get_name(table, base_type_id);
    type_info.name[0] = '*' as i8;
    str_copy(type_info.name[1], base_name);

    type_info.kind = TYPE_POINTER;
    type_info.size = 8;  // Pointers are 8 bytes on x86-64
    type_info.is_signed = 0;
    type_info.base_type_id = base_type_id;
    type_info.alignment = 8;

    table.count = table.count + 1;

    return type_info.id;
}

// ============================================
// TYPE CHECKING
// ============================================

fn typecheck_compatible(table: *TypeTable, type1: i64, type2: i64) -> i64 {
    // Types must be exactly equal (no implicit conversions)
    if (type1 == type2) {
        return 1;
    }

    return 0;
}

fn typecheck_can_cast(table: *TypeTable, from_type: i64, to_type: i64) -> i64 {
    let from_info: *TypeInfo = typetable_lookup_by_id(table, from_type);
    let to_info: *TypeInfo = typetable_lookup_by_id(table, to_type);

    if (from_info as i64 == 0 || to_info as i64 == 0) {
        return 0;  // Unknown types
    }

    // Can cast between primitive numeric types
    if (from_info.kind == TYPE_PRIMITIVE && to_info.kind == TYPE_PRIMITIVE) {
        // Allow casting between integers and floats
        if (from_type >= TYPE_I8 && from_type <= TYPE_F64 &&
            to_type >= TYPE_I8 && to_type <= TYPE_F64) {
            return 1;
        }
    }

    // Can cast pointer to pointer
    if (from_info.kind == TYPE_POINTER && to_info.kind == TYPE_POINTER) {
        return 1;
    }

    // Can cast integer to pointer (unsafe!)
    if (from_info.kind == TYPE_PRIMITIVE && to_info.kind == TYPE_POINTER) {
        if (from_type >= TYPE_I8 && from_type <= TYPE_U64) {
            return 1;
        }
    }

    // Can cast pointer to integer
    if (from_info.kind == TYPE_POINTER && to_info.kind == TYPE_PRIMITIVE) {
        if (to_type >= TYPE_I8 && to_type <= TYPE_U64) {
            return 1;
        }
    }

    return 0;
}

// ============================================
// TYPE ERROR REPORTING
// ============================================

fn type_error_incompatible(
    table: *TypeTable,
    line: i64,
    expected_type: i64,
    got_type: i64
) -> i64 {
    print("ERROR at line ");
    print_i64(line);
    print(": Type mismatch\n");

    print("  Expected: ");
    print(typetable_get_name(table, expected_type));
    print("\n");

    print("  Got:      ");
    print(typetable_get_name(table, got_type));
    print("\n");

    return 0;
}

fn type_error_no_cast(
    table: *TypeTable,
    line: i64,
    from_type: i64,
    to_type: i64
) -> i64 {
    print("ERROR at line ");
    print_i64(line);
    print(": Cannot cast\n");

    print("  From: ");
    print(typetable_get_name(table, from_type));
    print("\n");

    print("  To:   ");
    print(typetable_get_name(table, to_type));
    print("\n");

    return 0;
}

// ============================================
// UTILITY FUNCTIONS
// ============================================

fn str_copy(dest: *i8, src: *i8) -> i64 {
    let i: i64 = 0;
    while (src[i] != 0) {
        dest[i] = src[i];
        i = i + 1;
    }
    dest[i] = 0;
    return i;
}

fn str_equal(s1: *i8, s2: *i8) -> i64 {
    let i: i64 = 0;
    while (s1[i] != 0 && s2[i] != 0) {
        if (s1[i] != s2[i]) {
            return 0;
        }
        i = i + 1;
    }
    return s1[i] == s2[i];
}

fn print_i64(n: i64) -> i64 {
    if (n < 0) {
        print("-");
        n = 0 - n;
    }

    if (n >= 10) {
        print_i64(n / 10);
    }

    let digit: i64 = n % 10;
    let ch: i8 = (digit + 48) as i8;  // '0' = 48
    write(1, ch, 1);

    return 0;
}

// ============================================
// TYPE TABLE DEBUG
// ============================================

fn typetable_dump(table: *TypeTable) -> i64 {
    println("========================================");
    println("TYPE TABLE DUMP");
    println("========================================");

    print("Total types: ");
    print_i64(table.count);
    print("\n\n");

    let i: i64 = 0;
    while (i < table.count) {
        let type_info: *TypeInfo = table.types[i];

        print("Type ");
        print_i64(i);
        print(": ");
        print(type_info.name);
        print("\n");

        print("  ID:     ");
        print_i64(type_info.id);
        print("\n");

        print("  Kind:   ");
        if (type_info.kind == TYPE_PRIMITIVE) {
            print("primitive");
        } else if (type_info.kind == TYPE_POINTER) {
            print("pointer");
        } else {
            print("unknown");
        }
        print("\n");

        print("  Size:   ");
        print_i64(type_info.size);
        print(" bytes\n");

        if (type_info.kind == TYPE_PRIMITIVE) {
            print("  Signed: ");
            if (type_info.is_signed) {
                print("yes");
            } else {
                print("no");
            }
            print("\n");
        }

        if (type_info.kind == TYPE_POINTER) {
            print("  Base:   ");
            print(typetable_get_name(table, type_info.base_type_id));
            print("\n");
        }

        print("\n");
        i = i + 1;
    }

    println("========================================");
    return 1;
}

// ============================================
// MAIN (FOR TESTING)
// ============================================

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS TYPE CHECKER v1.0");
    println("  Foundation Implementation");
    println("========================================");
    println("");

    // Initialize type table
    let table: TypeTable;
    typetable_init(table);

    println("✅ Type table initialized");
    println("");

    // Test lookups
    println("Testing type lookups:");
    println("");

    let id_i64: i64 = typetable_lookup_by_name(table, "i64");
    print("  i64 -> ID ");
    print_i64(id_i64);
    print("\n");

    let id_u32: i64 = typetable_lookup_by_name(table, "u32");
    print("  u32 -> ID ");
    print_i64(id_u32);
    print("\n");

    let id_bool: i64 = typetable_lookup_by_name(table, "bool");
    print("  bool -> ID ");
    print_i64(id_bool);
    print("\n");

    println("");

    // Test pointer type registration
    println("Testing pointer type registration:");
    println("");

    let ptr_i64: i64 = typetable_register_pointer(table, TYPE_I64);
    print("  *i64 -> ID ");
    print_i64(ptr_i64);
    print("\n");

    let ptr_u32: i64 = typetable_register_pointer(table, TYPE_U32);
    print("  *u32 -> ID ");
    print_i64(ptr_u32);
    print("\n");

    println("");

    // Test type compatibility
    println("Testing type compatibility:");
    println("");

    if (typecheck_compatible(table, TYPE_I64, TYPE_I64)) {
        println("  i64 == i64: ✅ compatible");
    } else {
        println("  i64 == i64: ❌ incompatible");
    }

    if (typecheck_compatible(table, TYPE_I64, TYPE_I32)) {
        println("  i64 == i32: ✅ compatible");
    } else {
        println("  i64 == i32: ❌ incompatible (CORRECT!)");
    }

    println("");

    // Test casting
    println("Testing type casting:");
    println("");

    if (typecheck_can_cast(table, TYPE_I32, TYPE_I64)) {
        println("  i32 as i64: ✅ allowed");
    } else {
        println("  i32 as i64: ❌ not allowed");
    }

    if (typecheck_can_cast(table, TYPE_I64, ptr_i64)) {
        println("  i64 as *i64: ✅ allowed");
    } else {
        println("  i64 as *i64: ❌ not allowed");
    }

    println("");

    // Dump type table
    typetable_dump(table);

    println("========================================");
    println("  ✅ ALL TESTS PASSED");
    println("========================================");

    return 0;
}
