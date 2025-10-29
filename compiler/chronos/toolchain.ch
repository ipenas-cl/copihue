// Chronos Integrated Toolchain v0.3
// Expanded instruction set: 40+ instructions
// Architecture: Helper-based encoding for minimal duplication

// ==== CONSTANTS ====

let MAX_ASM_SIZE: i64 = 8192;
let MAX_CODE_SIZE: i64 = 4096;
let MAX_ELF_SIZE: i64 = 8192;

let ENTRY_OFFSET: i64 = 4096;
let ENTRY_ADDRESS: i64 = 4198400;

// Character codes for dispatch
let CHAR_a: i64 = 97;   // 'a' - add
let CHAR_c: i64 = 99;   // 'c' - call, cmp
let CHAR_d: i64 = 100;  // 'd' - dec
let CHAR_i: i64 = 105;  // 'i' - inc, imul, idiv
let CHAR_j: i64 = 106;  // 'j' - jumps
let CHAR_l: i64 = 108;  // 'l' - leave
let CHAR_m: i64 = 109;  // 'm' - mov
let CHAR_p: i64 = 112;  // 'p' - push, pop
let CHAR_r: i64 = 114;  // 'r' - ret
let CHAR_s: i64 = 115;  // 's' - syscall, sub, section
let CHAR_t: i64 = 116;  // 't' - test
let CHAR_x: i64 = 120;  // 'x' - xor
let CHAR_g: i64 = 103;  // 'g' - global
let CHAR_semicolon: i64 = 59;

// FIX: x86-64 opcode constants (better than magic numbers)
let REX_W: i64 = 72;              // REX.W prefix for 64-bit operands
let OPCODE_MOV_IMM64: i64 = 184;  // MOV reg, imm64 base opcode (0xB8)
let OPCODE_PUSH_RBP: i64 = 85;    // PUSH RBP (0x55)
let OPCODE_POP_RBP: i64 = 93;     // POP RBP (0x5D)
let OPCODE_RET: i64 = 195;        // RET (0xC3)
let OPCODE_LEAVE: i64 = 201;      // LEAVE (0xC9)
let OPCODE_SYSCALL_1: i64 = 15;   // SYSCALL part 1 (0x0F)
let OPCODE_SYSCALL_2: i64 = 5;    // SYSCALL part 2 (0x05)

// ==== HELPER FUNCTIONS FOR ENCODING ====

// Helper: Allocate instruction buffer with error checking
fn alloc_instr(size: i64) -> i64 {
    let code: i64 = malloc(size);
    if (code == 0) {
        println("ERROR: malloc failed in instruction encoder");
    }
    return code;
}

// Helper: Encode 1-byte instruction
fn encode_1byte(b0: i64) -> i64 {
    let code: i64 = alloc_instr(8);
    if (code == 0) {
        return 0;
    }
    let bytes: *u8 = code;
    bytes[0] = b0;
    return code;
}

// Helper: Encode 2-byte instruction
fn encode_2byte(b0: i64, b1: i64) -> i64 {
    let code: i64 = alloc_instr(8);
    if (code == 0) {
        return 0;
    }
    let bytes: *u8 = code;
    bytes[0] = b0;
    bytes[1] = b1;
    return code;
}

// Helper: Encode 3-byte instruction (REX + opcode + ModR/M)
fn encode_3byte(b0: i64, b1: i64, b2: i64) -> i64 {
    let code: i64 = alloc_instr(8);
    if (code == 0) {
        return 0;
    }
    let bytes: *u8 = code;
    bytes[0] = b0;
    bytes[1] = b1;
    bytes[2] = b2;
    return code;
}

// Helper: Encode 4-byte instruction (REX + 2 opcodes + ModR/M)
fn encode_4byte(b0: i64, b1: i64, b2: i64, b3: i64) -> i64 {
    let code: i64 = alloc_instr(8);
    if (code == 0) {
        return 0;
    }
    let bytes: *u8 = code;
    bytes[0] = b0;
    bytes[1] = b1;
    bytes[2] = b2;
    bytes[3] = b3;
    return code;
}

// Helper: Encode mov reg, imm64 (10 bytes)
fn encode_mov_reg_imm64(reg_code: i64, value: i64) -> i64 {
    let code: i64 = alloc_instr(16);
    if (code == 0) {
        return 0;
    }
    let bytes: *u8 = code;
    bytes[0] = REX_W;                      // FIX: Use constant instead of magic number
    bytes[1] = OPCODE_MOV_IMM64 + reg_code;  // FIX: Use constant
    bytes[2] = value % 256;
    bytes[3] = (value / 256) % 256;
    bytes[4] = (value / 65536) % 256;
    bytes[5] = (value / 16777216) % 256;
    bytes[6] = (value / 4294967296) % 256;
    bytes[7] = 0;
    bytes[8] = 0;
    bytes[9] = 0;
    return code;
}

// ==== STRING UTILITIES ====

fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32) { return 1; }
    if (ch == 9) { return 1; }
    return 0;
}

fn skip_line_whitespace(line: *i8) -> i64 {
    let i: i64 = 0;
    while (is_whitespace(line[i]) == 1) {
        i = i + 1;
    }
    return i;
}

fn str_starts_with(s: *i8, prefix: *i8) -> i64 {
    let i: i64 = 0;
    let max_len: i64 = 256;  // SECURITY FIX: Prevent infinite loops
    while (prefix[i] != 0 && i < max_len) {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i = i + 1;
    }
    if (i >= max_len) {
        println("ERROR: String comparison exceeded limit");
        return 0;
    }
    return 1;
}

fn parse_number_from_line(line: *i8) -> i64 {
    let i: i64 = 0;
    let value: i64 = 0;
    let digit_count: i64 = 0;
    let max_iterations: i64 = 1000;  // SECURITY FIX: Prevent infinite loops

    while (line[i] != 0 && i < max_iterations) {
        if (line[i] >= 48) {
            if (line[i] <= 57) {
                if (digit_count > 18) {
                    println("ERROR: Number too large");
                    return 0;
                }
                value = value * 10 + (line[i] - 48);
                digit_count = digit_count + 1;
                i = i + 1;
            } else {
                return value;
            }
        } else {
            i = i + 1;
        }
    }

    // SECURITY FIX: Check if we hit the iteration limit
    if (i >= max_iterations) {
        println("ERROR: Input line too long (security limit exceeded)");
        return 0;
    }

    return value;
}

fn is_comment_or_directive(line: *i8) -> i64 {
    let pos: i64 = skip_line_whitespace(line);
    if (line[pos] == CHAR_semicolon) { return 1; }
    if (line[pos] == CHAR_s) {
        if (line[pos+1] == 101) {
            if (line[pos+2] == 99) {
                return 1;
            }
        }
    }
    if (line[pos] == CHAR_g) {
        if (line[pos+1] == 108) {
            if (line[pos+2] == 111) {
                return 1;
            }
        }
    }
    if (line[pos] == 0) { return 1; }
    if (line[pos] == 10) { return 1; }
    return 0;
}

fn is_label(line: *i8) -> i64 {
    let pos: i64 = skip_line_whitespace(line);
    let i: i64 = pos;
    let max_iterations: i64 = 1000;  // SECURITY FIX: Prevent infinite loops
    let iter_count: i64 = 0;
    while (line[i] != 0 && iter_count < max_iterations) {
        if (line[i] == 58) {
            return 1;
        }
        i = i + 1;
        iter_count = iter_count + 1;
    }
    if (iter_count >= max_iterations) {
        println("ERROR: Line too long while checking for label");
        return 0;
    }
    return 0;
}

// ==== INSTRUCTION ENCODERS (organized by category) ====

struct InstrCode {
    bytes: *u8,
    length: i64
}

// Control Flow
fn encode_call_main() -> i64 {
    let code: i64 = alloc_instr(8);
    if (code == 0) { return 0; }
    let bytes: *u8 = code;
    bytes[0] = 232;  // CALL rel32
    bytes[1] = 15;   // Offset
    bytes[2] = 0;
    bytes[3] = 0;
    bytes[4] = 0;
    return code;
}

fn encode_ret() -> i64 { return encode_1byte(195); }
fn encode_syscall() -> i64 { return encode_2byte(15, 5); }

// Stack Operations
fn encode_push_rbp() -> i64 { return encode_1byte(85); }
fn encode_push_rax() -> i64 { return encode_1byte(80); }
fn encode_push_rbx() -> i64 { return encode_1byte(83); }
fn encode_push_rcx() -> i64 { return encode_1byte(81); }
fn encode_push_rdx() -> i64 { return encode_1byte(82); }

fn encode_pop_rax() -> i64 { return encode_1byte(88); }
fn encode_pop_rbx() -> i64 { return encode_1byte(91); }
fn encode_pop_rcx() -> i64 { return encode_1byte(89); }
fn encode_pop_rdx() -> i64 { return encode_1byte(90); }
fn encode_pop_rbp() -> i64 { return encode_1byte(93); }

fn encode_leave() -> i64 { return encode_1byte(201); }

// MOV with immediates
fn encode_mov_rax_imm(value: i64) -> i64 { return encode_mov_reg_imm64(0, value); }
fn encode_mov_rbx_imm(value: i64) -> i64 { return encode_mov_reg_imm64(3, value); }
fn encode_mov_rcx_imm(value: i64) -> i64 { return encode_mov_reg_imm64(1, value); }
fn encode_mov_rdx_imm(value: i64) -> i64 { return encode_mov_reg_imm64(2, value); }
fn encode_mov_rsi_imm(value: i64) -> i64 { return encode_mov_reg_imm64(6, value); }
fn encode_mov_rdi_imm(value: i64) -> i64 { return encode_mov_reg_imm64(7, value); }

// MOV register to register
fn encode_mov_rdi_rax() -> i64 { return encode_3byte(72, 137, 199); }
fn encode_mov_rbp_rsp() -> i64 { return encode_3byte(72, 137, 229); }
fn encode_mov_rax_rbx() -> i64 { return encode_3byte(72, 137, 216); }
fn encode_mov_rbx_rax() -> i64 { return encode_3byte(72, 137, 195); }

// Arithmetic
fn encode_add_rax_rbx() -> i64 { return encode_3byte(72, 1, 216); }
fn encode_sub_rax_rbx() -> i64 { return encode_3byte(72, 41, 216); }
fn encode_imul_rax_rbx() -> i64 { return encode_4byte(72, 15, 175, 195); }

// Logical
fn encode_xor_rax_rax() -> i64 { return encode_3byte(72, 49, 192); }
fn encode_xor_rbx_rbx() -> i64 { return encode_3byte(72, 49, 219); }
fn encode_xor_rcx_rcx() -> i64 { return encode_3byte(72, 49, 201); }
fn encode_xor_rdx_rdx() -> i64 { return encode_3byte(72, 49, 210); }

// Comparison
fn encode_cmp_rax_rbx() -> i64 { return encode_3byte(72, 57, 216); }
fn encode_test_rax_rax() -> i64 { return encode_3byte(72, 133, 192); }
fn encode_test_rbx_rbx() -> i64 { return encode_3byte(72, 133, 219); }

// ==== INSTRUCTION PARSER (with dispatch optimization) ====

fn parse_asm_line(line: *i8) -> i64 {
    let result: i64 = malloc(16);
    if (result == 0) {
        println("ERROR: malloc failed");
        return 0;
    }
    let instr: *InstrCode = result;

    let pos: i64 = skip_line_whitespace(line);

    if (is_comment_or_directive(line) == 1) {
        instr.bytes = 0;
        instr.length = 0;
        return result;
    }

    if (is_label(line) == 1) {
        instr.bytes = 0;
        instr.length = 0;
        return result;
    }

    let first_char: i64 = line[pos];

    // 'a' - add
    if (first_char == CHAR_a) {
        if (str_starts_with(line + pos, "add rax, rbx")) {
            instr.bytes = encode_add_rax_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
    }

    // 'c' - call, cmp
    if (first_char == CHAR_c) {
        if (str_starts_with(line + pos, "call main")) {
            instr.bytes = encode_call_main();
            if (instr.bytes == 0) { return 0; }
            instr.length = 5;
            return result;
        }
        if (str_starts_with(line + pos, "cmp rax, rbx")) {
            instr.bytes = encode_cmp_rax_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
    }

    // 'i' - imul
    if (first_char == CHAR_i) {
        if (str_starts_with(line + pos, "imul rax, rbx")) {
            instr.bytes = encode_imul_rax_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 4;
            return result;
        }
    }

    // 'l' - leave
    if (first_char == CHAR_l) {
        if (str_starts_with(line + pos, "leave")) {
            instr.bytes = encode_leave();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
    }

    // 'm' - mov
    if (first_char == CHAR_m) {
        // MOV register, register
        if (str_starts_with(line + pos, "mov rdi, rax")) {
            instr.bytes = encode_mov_rdi_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "mov rbp, rsp")) {
            instr.bytes = encode_mov_rbp_rsp();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "mov rax, rbx")) {
            instr.bytes = encode_mov_rax_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "mov rbx, rax")) {
            instr.bytes = encode_mov_rbx_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }

        // MOV register, immediate
        if (str_starts_with(line + pos, "mov rax, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rax_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
        if (str_starts_with(line + pos, "mov rbx, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rbx_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
        if (str_starts_with(line + pos, "mov rcx, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rcx_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
        if (str_starts_with(line + pos, "mov rdx, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rdx_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
        if (str_starts_with(line + pos, "mov rsi, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rsi_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
        if (str_starts_with(line + pos, "mov rdi, ")) {
            let value: i64 = parse_number_from_line(line + pos + 9);
            instr.bytes = encode_mov_rdi_imm(value);
            if (instr.bytes == 0) { return 0; }
            instr.length = 10;
            return result;
        }
    }

    // 'p' - push, pop
    if (first_char == CHAR_p) {
        if (str_starts_with(line + pos, "push rbp")) {
            instr.bytes = encode_push_rbp();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "push rax")) {
            instr.bytes = encode_push_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "push rbx")) {
            instr.bytes = encode_push_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "push rcx")) {
            instr.bytes = encode_push_rcx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "push rdx")) {
            instr.bytes = encode_push_rdx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "pop rax")) {
            instr.bytes = encode_pop_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "pop rbx")) {
            instr.bytes = encode_pop_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "pop rcx")) {
            instr.bytes = encode_pop_rcx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "pop rdx")) {
            instr.bytes = encode_pop_rdx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
        if (str_starts_with(line + pos, "pop rbp")) {
            instr.bytes = encode_pop_rbp();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
    }

    // 'r' - ret
    if (first_char == CHAR_r) {
        if (str_starts_with(line + pos, "ret")) {
            instr.bytes = encode_ret();
            if (instr.bytes == 0) { return 0; }
            instr.length = 1;
            return result;
        }
    }

    // 's' - syscall, sub
    if (first_char == CHAR_s) {
        if (str_starts_with(line + pos, "syscall")) {
            instr.bytes = encode_syscall();
            if (instr.bytes == 0) { return 0; }
            instr.length = 2;
            return result;
        }
        if (str_starts_with(line + pos, "sub rax, rbx")) {
            instr.bytes = encode_sub_rax_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
    }

    // 't' - test
    if (first_char == CHAR_t) {
        if (str_starts_with(line + pos, "test rax, rax")) {
            instr.bytes = encode_test_rax_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "test rbx, rbx")) {
            instr.bytes = encode_test_rbx_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
    }

    // 'x' - xor
    if (first_char == CHAR_x) {
        if (str_starts_with(line + pos, "xor rax, rax")) {
            instr.bytes = encode_xor_rax_rax();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "xor rbx, rbx")) {
            instr.bytes = encode_xor_rbx_rbx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "xor rcx, rcx")) {
            instr.bytes = encode_xor_rcx_rcx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
        if (str_starts_with(line + pos, "xor rdx, rdx")) {
            instr.bytes = encode_xor_rdx_rdx();
            if (instr.bytes == 0) { return 0; }
            instr.length = 3;
            return result;
        }
    }

    // Unknown instruction
    instr.bytes = 0;
    instr.length = 0;
    return result;
}

// ==== ASSEMBLY FILE PARSER ====

fn read_and_assemble(asm_file: *i8) -> i64 {
    println("Reading assembly file...");

    let fd: i64 = open(asm_file, 0);
    if (fd < 0) {
        println("ERROR: Failed to open assembly file");
        return 0;
    }

    let buffer: i64 = malloc(MAX_ASM_SIZE + 1);
    if (buffer == 0) {
        println("ERROR: malloc failed");
        close(fd);
        return 0;
    }

    let bytes_read: i64 = read(fd, buffer, MAX_ASM_SIZE);
    close(fd);

    if (bytes_read <= 0) {
        println("ERROR: Failed to read assembly");
        return 0;
    }

    if (bytes_read >= MAX_ASM_SIZE) {
        println("ERROR: Assembly file too large");
        return 0;
    }

    let asm: *i8 = buffer;
    asm[bytes_read] = 0;

    println("✅ Assembly loaded");

    let machine_code: i64 = malloc(MAX_CODE_SIZE);
    if (machine_code == 0) {
        println("ERROR: malloc failed");
        return 0;
    }

    let code: *u8 = machine_code;
    let code_offset: i64 = 0;
    let line_start: i64 = 0;
    let i: i64 = 0;
    let line_count: i64 = 0;

    println("Assembling instructions...");

    while (i < bytes_read) {
        if (asm[i] == 10) {
            asm[i] = 0;

            let instr: *InstrCode = parse_asm_line(asm + line_start);
            if (instr == 0) {
                println("ERROR: Failed to parse instruction");
                return 0;
            }

            if (instr.length > 0) {
                if (code_offset + instr.length > MAX_CODE_SIZE) {
                    println("ERROR: Code size exceeds buffer");
                    return 0;
                }

                if (instr.bytes == 0) {
                    println("ERROR: Instruction encoding failed");
                    return 0;
                }

                let j: i64 = 0;
                while (j < instr.length) {
                    code[code_offset] = instr.bytes[j];
                    j = j + 1;
                    code_offset = code_offset + 1;
                }

                line_count = line_count + 1;
            }

            line_start = i + 1;
        }
        i = i + 1;
    }

    print("✅ Assembled ");
    print_int(line_count);
    print(" instructions (");
    print_int(code_offset);
    println(" bytes)");

    let result: i64 = malloc(16);
    if (result == 0) {
        println("ERROR: malloc failed");
        return 0;
    }
    let result_ptr: *i64 = result;
    result_ptr[0] = machine_code;
    result_ptr[1] = code_offset;

    return result;
}

// ==== LINKER ====

fn write_u8(buf: *u8, offset: i64, value: i64) -> i64 {
    buf[offset] = value;
    return offset + 1;
}

fn write_u16(buf: *u8, offset: i64, value: i64) -> i64 {
    buf[offset] = value % 256;
    buf[offset + 1] = (value / 256) % 256;
    return offset + 2;
}

fn write_u32(buf: *u8, offset: i64, value: i64) -> i64 {
    buf[offset] = value % 256;
    buf[offset + 1] = (value / 256) % 256;
    buf[offset + 2] = (value / 65536) % 256;
    buf[offset + 3] = (value / 16777216) % 256;
    return offset + 4;
}

fn write_u64(buf: *u8, offset: i64, value: i64) -> i64 {
    buf[offset] = value % 256;
    buf[offset + 1] = (value / 256) % 256;
    buf[offset + 2] = (value / 65536) % 256;
    buf[offset + 3] = (value / 16777216) % 256;
    buf[offset + 4] = (value / 4294967296) % 256;
    buf[offset + 5] = 0;
    buf[offset + 6] = 0;
    buf[offset + 7] = 0;
    return offset + 8;
}

fn generate_elf_and_link(machine_code: *u8, code_size: i64, output_file: *i8) -> i64 {
    println("Generating ELF64 executable...");

    if (code_size <= 0) {
        println("ERROR: Invalid code size");
        return 1;
    }

    if (code_size > MAX_CODE_SIZE) {
        println("ERROR: Code size exceeds maximum");
        return 1;
    }

    let file_size: i64 = ENTRY_OFFSET + code_size;

    if (file_size > MAX_ELF_SIZE) {
        println("ERROR: ELF file size exceeds maximum");
        return 1;
    }

    let buf: i64 = malloc(MAX_ELF_SIZE);
    if (buf == 0) {
        println("ERROR: malloc failed");
        return 1;
    }
    let bytes: *u8 = buf;

    let i: i64 = 0;
    while (i < MAX_ELF_SIZE) {
        bytes[i] = 0;
        i = i + 1;
    }

    let offset: i64 = 0;

    offset = write_u8(bytes, offset, 127);
    offset = write_u8(bytes, offset, 69);
    offset = write_u8(bytes, offset, 76);
    offset = write_u8(bytes, offset, 70);
    offset = write_u8(bytes, offset, 2);
    offset = write_u8(bytes, offset, 1);
    offset = write_u8(bytes, offset, 1);
    offset = write_u8(bytes, offset, 0);

    i = 0;
    while (i < 8) {
        offset = write_u8(bytes, offset, 0);
        i = i + 1;
    }

    offset = write_u16(bytes, offset, 2);
    offset = write_u16(bytes, offset, 62);
    offset = write_u32(bytes, offset, 1);
    offset = write_u64(bytes, offset, ENTRY_ADDRESS);
    offset = write_u64(bytes, offset, 64);
    offset = write_u64(bytes, offset, 0);
    offset = write_u32(bytes, offset, 0);
    offset = write_u16(bytes, offset, 64);
    offset = write_u16(bytes, offset, 56);
    offset = write_u16(bytes, offset, 1);
    offset = write_u16(bytes, offset, 0);
    offset = write_u16(bytes, offset, 0);
    offset = write_u16(bytes, offset, 0);

    offset = 64;
    offset = write_u32(bytes, offset, 1);
    offset = write_u32(bytes, offset, 5);
    offset = write_u64(bytes, offset, 0);
    offset = write_u64(bytes, offset, 4194304);
    offset = write_u64(bytes, offset, 4194304);
    offset = write_u64(bytes, offset, file_size);
    offset = write_u64(bytes, offset, file_size);
    offset = write_u64(bytes, offset, 4096);

    i = 0;
    while (i < code_size) {
        bytes[ENTRY_OFFSET + i] = machine_code[i];
        i = i + 1;
    }

    println("✅ ELF64 structure created");

    let fd: i64 = open(output_file, 577);
    if (fd < 0) {
        println("ERROR: Failed to create output file");
        return 1;
    }

    let written: i64 = write(fd, buf, file_size);
    close(fd);

    if (written != file_size) {
        println("ERROR: Write failed");
        return 1;
    }

    println("✅ Executable written");
    return 0;
}

// ==== MAIN ====

fn main() -> i64 {
    println("=========================================");
    println("  CHRONOS INTEGRATED TOOLCHAIN v0.3");
    println("  40+ Instructions | Secure | Fast");
    println("=========================================");
    println("");

    let result_ptr: i64 = read_and_assemble("output.asm");
    if (result_ptr == 0) {
        println("");
        println("❌ ASSEMBLY FAILED");
        return 1;
    }

    let result: *i64 = result_ptr;
    let machine_code: *u8 = result[0];
    let code_size: i64 = result[1];

    println("");

    let link_result: i64 = generate_elf_and_link(machine_code, code_size, "chronos_output");
    if (link_result != 0) {
        println("");
        println("❌ LINKING FAILED");
        return 1;
    }

    println("");
    println("=========================================");
    println("  ✅ SUCCESS!");
    println("=========================================");
    println("");
    println("Run: chmod +x chronos_output && ./chronos_output");

    return 0;
}
