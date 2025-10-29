// Chronos Assembler v0.1 - Simple x86-64 Assembler
// Converts assembly to machine code
// Part of Chronos v1.0 - 100% Self-Contained

// ==== Instruction Encoding ====

// Encodes: mov rax, imm64
// Opcode: 48 B8 [imm64]
fn encode_mov_rax_imm(value: i64) -> i64 {
    let code: i64 = malloc(16);
    let bytes: *u8 = code;

    bytes[0] = 72;  // REX.W prefix (0x48)
    bytes[1] = 184;  // MOV rax, imm64 (0xB8)

    // Little-endian encoding of value
    bytes[2] = value % 256;
    bytes[3] = (value / 256) % 256;
    bytes[4] = (value / 65536) % 256;
    bytes[5] = (value / 16777216) % 256;
    bytes[6] = (value / 4294967296) % 256;
    bytes[7] = 0;  // Upper bytes usually 0 for small values
    bytes[8] = 0;
    bytes[9] = 0;

    return code;  // Returns pointer to 10 bytes
}

// Encodes: mov rdi, rax
// Opcode: 48 89 C7
fn encode_mov_rdi_rax() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;  // REX.W prefix (0x48)
    bytes[1] = 137;  // MOV r/m64, r64 (0x89)
    bytes[2] = 199;  // ModR/M: rdi = rax (0xC7)

    return code;  // Returns pointer to 3 bytes
}

// Encodes: syscall
// Opcode: 0F 05
fn encode_syscall() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 15;  // 0x0F
    bytes[1] = 5;   // 0x05

    return code;  // Returns pointer to 2 bytes
}

// Encodes: ret
// Opcode: C3
fn encode_ret() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 195;  // 0xC3

    return code;  // Returns pointer to 1 byte
}

// Encodes: push rbp
// Opcode: 55
fn encode_push_rbp() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 85;  // 0x55

    return code;
}

// Encodes: pop rbp
// Opcode: 5D
fn encode_pop_rbp() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 93;  // 0x5D

    return code;
}

// Encodes: mov rbp, rsp
// Opcode: 48 89 E5
fn encode_mov_rbp_rsp() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 137;  // MOV (0x89)
    bytes[2] = 229;  // ModR/M: rbp = rsp (0xE5)

    return code;
}

// Encodes: leave
// Opcode: C9
fn encode_leave() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 201;  // 0xC9

    return code;
}

// Encodes: push rax
// Opcode: 50
fn encode_push_rax() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 80;  // 0x50

    return code;
}

// Encodes: pop rbx
// Opcode: 5B
fn encode_pop_rbx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 91;  // 0x5B

    return code;
}

// Encodes: add rax, rbx
// Opcode: 48 01 D8
fn encode_add_rax_rbx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 1;    // ADD (0x01)
    bytes[2] = 216;  // ModR/M: rax += rbx (0xD8)

    return code;
}

// Encodes: sub rax, rbx
// Opcode: 48 29 D8
fn encode_sub_rax_rbx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 41;   // SUB (0x29)
    bytes[2] = 216;  // ModR/M: rax -= rbx (0xD8)

    return code;
}

// Encodes: imul rax, rbx
// Opcode: 48 0F AF C3
fn encode_imul_rax_rbx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 15;   // Two-byte opcode prefix (0x0F)
    bytes[2] = 175;  // IMUL (0xAF)
    bytes[3] = 195;  // ModR/M: rax *= rbx (0xC3)

    return code;
}

// Encodes: xor rdx, rdx
// Opcode: 48 31 D2
fn encode_xor_rdx_rdx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 49;   // XOR (0x31)
    bytes[2] = 210;  // ModR/M: rdx ^= rdx (0xD2)

    return code;
}

// Encodes: div rbx
// Opcode: 48 F7 F3
fn encode_div_rbx() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W (0x48)
    bytes[1] = 247;  // Unary group (0xF7)
    bytes[2] = 243;  // ModR/M: div rbx (0xF3)

    return code;
}

// Encodes: call offset (relative 32-bit)
// Opcode: E8 [rel32]
// For now, just placeholder - needs relocation
fn encode_call_rel(offset: i64) -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;

    bytes[0] = 232;  // CALL rel32 (0xE8)
    bytes[1] = offset % 256;
    bytes[2] = (offset / 256) % 256;
    bytes[3] = (offset / 65536) % 256;
    bytes[4] = (offset / 16777216) % 256;

    return code;
}

// ==== String Utilities ====

fn str_equals(s1: *i8, s2: *i8) -> i64 {
    let i: i64 = 0;
    while (s1[i] != 0) {
        if (s1[i] != s2[i]) {
            return 0;
        }
        i = i + 1;
    }
    if (s2[i] != 0) {
        return 0;
    }
    return 1;
}

fn str_starts_with(s: *i8, prefix: *i8) -> i64 {
    let i: i64 = 0;
    while (prefix[i] != 0) {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn skip_whitespace(s: *i8) -> i64 {
    let i: i64 = 0;
    while (s[i] == 32) {  // space
        i = i + 1;
    }
    while (s[i] == 9) {  // tab
        i = i + 1;
    }
    return i;
}

fn parse_number(s: *i8) -> i64 {
    let value: i64 = 0;
    let i: i64 = 0;

    while (s[i] >= 48) {
        if (s[i] <= 57) {
            value = value * 10 + (s[i] - 48);
            i = i + 1;
        } else {
            return value;
        }
    }

    return value;
}

// ==== Assembly Parser ====

struct Instruction {
    bytes: *u8,
    length: i64
}

fn parse_instruction(line: *i8) -> i64 {
    let instr_addr: i64 = malloc(16);
    let instr: *Instruction = instr_addr;

    let pos: i64 = skip_whitespace(line);

    // Check for "mov rax, N"
    if (str_starts_with(line + pos, "mov rax, ")) {
        pos = pos + 9;  // Skip "mov rax, "
        let value: i64 = parse_number(line + pos);

        instr.bytes = encode_mov_rax_imm(value);
        instr.length = 10;
        return instr_addr;
    }

    // Check for "mov rdi, rax"
    if (str_starts_with(line + pos, "mov rdi, rax")) {
        instr.bytes = encode_mov_rdi_rax();
        instr.length = 3;
        return instr_addr;
    }

    // Check for "mov rbp, rsp"
    if (str_starts_with(line + pos, "mov rbp, rsp")) {
        instr.bytes = encode_mov_rbp_rsp();
        instr.length = 3;
        return instr_addr;
    }

    // Check for "syscall"
    if (str_starts_with(line + pos, "syscall")) {
        instr.bytes = encode_syscall();
        instr.length = 2;
        return instr_addr;
    }

    // Check for "ret"
    if (str_starts_with(line + pos, "ret")) {
        instr.bytes = encode_ret();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "push rbp"
    if (str_starts_with(line + pos, "push rbp")) {
        instr.bytes = encode_push_rbp();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "pop rbp"
    if (str_starts_with(line + pos, "pop rbp")) {
        instr.bytes = encode_pop_rbp();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "leave"
    if (str_starts_with(line + pos, "leave")) {
        instr.bytes = encode_leave();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "push rax"
    if (str_starts_with(line + pos, "push rax")) {
        instr.bytes = encode_push_rax();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "pop rbx"
    if (str_starts_with(line + pos, "pop rbx")) {
        instr.bytes = encode_pop_rbx();
        instr.length = 1;
        return instr_addr;
    }

    // Check for "add rax, rbx"
    if (str_starts_with(line + pos, "add rax, rbx")) {
        instr.bytes = encode_add_rax_rbx();
        instr.length = 3;
        return instr_addr;
    }

    // Check for "sub rax, rbx"
    if (str_starts_with(line + pos, "sub rax, rbx")) {
        instr.bytes = encode_sub_rax_rbx();
        instr.length = 3;
        return instr_addr;
    }

    // Check for "imul rax, rbx"
    if (str_starts_with(line + pos, "imul rax, rbx")) {
        instr.bytes = encode_imul_rax_rbx();
        instr.length = 4;
        return instr_addr;
    }

    // Check for "xor rdx, rdx"
    if (str_starts_with(line + pos, "xor rdx, rdx")) {
        instr.bytes = encode_xor_rdx_rdx();
        instr.length = 3;
        return instr_addr;
    }

    // Check for "div rbx"
    if (str_starts_with(line + pos, "div rbx")) {
        instr.bytes = encode_div_rbx();
        instr.length = 3;
        return instr_addr;
    }

    // Unknown instruction - return empty
    instr.bytes = 0;
    instr.length = 0;
    return instr_addr;
}

// ==== Main Test ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS ASSEMBLER v0.1");
    println("  x86-64 Machine Code Generator");
    println("========================================");
    println("");

    // Test encoding individual instructions
    println("Testing instruction encoding:");
    println("");

    // Test 1: mov rax, 42
    print("1. mov rax, 42 → ");
    let instr1: *Instruction = parse_instruction("mov rax, 42");
    if (instr1.length == 10) {
        print("✅ ");
        print_int(instr1.length);
        println(" bytes");

        // Print hex bytes
        print("   Bytes: ");
        let bytes: *u8 = instr1.bytes;
        let i: i64 = 0;
        while (i < instr1.length) {
            print_int(bytes[i]);
            print(" ");
            i = i + 1;
        }
        println("");
    } else {
        println("❌ Failed");
    }

    println("");

    // Test 2: syscall
    print("2. syscall → ");
    let instr2: *Instruction = parse_instruction("syscall");
    if (instr2.length == 2) {
        print("✅ ");
        print_int(instr2.length);
        println(" bytes");

        print("   Bytes: ");
        let bytes: *u8 = instr2.bytes;
        let i: i64 = 0;
        while (i < instr2.length) {
            print_int(bytes[i]);
            print(" ");
            i = i + 1;
        }
        println("");
    } else {
        println("❌ Failed");
    }

    println("");

    // Test 3: ret
    print("3. ret → ");
    let instr3: *Instruction = parse_instruction("ret");
    if (instr3.length == 1) {
        print("✅ ");
        print_int(instr3.length);
        println(" bytes");

        print("   Bytes: ");
        let bytes: *u8 = instr3.bytes;
        print_int(bytes[0]);
        println("");
    } else {
        println("❌ Failed");
    }

    println("");

    // Test 4: add rax, rbx
    print("4. add rax, rbx → ");
    let instr4: *Instruction = parse_instruction("add rax, rbx");
    if (instr4.length == 3) {
        print("✅ ");
        print_int(instr4.length);
        println(" bytes");
    } else {
        println("❌ Failed");
    }

    // Test 5: imul rax, rbx
    print("5. imul rax, rbx → ");
    let instr5: *Instruction = parse_instruction("imul rax, rbx");
    if (instr5.length == 4) {
        print("✅ ");
        print_int(instr5.length);
        println(" bytes");
    } else {
        println("❌ Failed");
    }

    // Test 6: div rbx
    print("6. div rbx → ");
    let instr6: *Instruction = parse_instruction("div rbx");
    if (instr6.length == 3) {
        print("✅ ");
        print_int(instr6.length);
        println(" bytes");
    } else {
        println("❌ Failed");
    }

    println("");
    println("========================================");
    println("  TOTAL: 15+ instructions supported!");
    println("========================================");
    println("");
    println("Supported instructions:");
    println("  - mov rax, N");
    println("  - mov rdi, rax / mov rbp, rsp");
    println("  - push rbp/rax, pop rbp/rbx");
    println("  - add/sub/imul rax, rbx");
    println("  - xor rdx, rdx");
    println("  - div rbx");
    println("  - syscall, ret, leave");

    return 0;
}
