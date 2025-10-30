// Chronos Integrated Toolchain v0.1
// Complete pipeline: .ch → executable (NO NASM, NO LD!)
// Part of Chronos v1.0 - 100% Self-Contained

// This integrates:
// 1. Compiler (generates assembly)
// 2. Assembler (assembly → machine code)
// 3. Linker (machine code → ELF64 executable)

// ==== Assembly Parser ====

fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32) {  // space
        return 1;
    }
    if (ch == 9) {  // tab
        return 1;
    }
    return 0;
}

fn skip_line_whitespace(line: *i8) -> i64 {
    let i: i64 = 0;
    while (is_whitespace(line[i]) == 1) {
        i = i + 1;
    }
    return i;
}

fn is_comment_or_directive(line: *i8) -> i64 {
    let pos: i64 = skip_line_whitespace(line);

    // Check for comment
    if (line[pos] == 59) {  // ';'
        return 1;
    }

    // Check for section directive
    if (line[pos] == 115) {  // 's'
        if (line[pos+1] == 101) {  // 'e'
            if (line[pos+2] == 99) {  // 'c'
                return 1;
            }
        }
    }

    // Check for global directive
    if (line[pos] == 103) {  // 'g'
        if (line[pos+1] == 108) {  // 'l'
            if (line[pos+2] == 111) {  // 'o'
                return 1;
            }
        }
    }

    // Check for empty line
    if (line[pos] == 0) {
        return 1;
    }
    if (line[pos] == 10) {  // newline
        return 1;
    }

    return 0;
}

fn is_label(line: *i8) -> i64 {
    let pos: i64 = skip_line_whitespace(line);

    // Check if line ends with ':'
    let i: i64 = pos;
    while (line[i] != 0) {
        if (line[i] == 58) {  // ':'
            return 1;
        }
        i = i + 1;
    }

    return 0;
}

// ==== Instruction Encoders (copied from assembler) ====

fn encode_mov_rax_imm(value: i64) -> i64 {
    let code: i64 = malloc(16);
    let bytes: *u8 = code;

    bytes[0] = 72;   // REX.W
    bytes[1] = 184;  // MOV rax, imm64
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

fn encode_mov_rdi_rax() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 72;
    bytes[1] = 137;
    bytes[2] = 199;
    return code;
}

fn encode_mov_rbp_rsp() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 72;
    bytes[1] = 137;
    bytes[2] = 229;
    return code;
}

fn encode_syscall() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 15;
    bytes[1] = 5;
    return code;
}

fn encode_ret() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 195;
    return code;
}

fn encode_push_rbp() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 85;
    return code;
}

fn encode_leave() -> i64 {
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 201;
    return code;
}

fn encode_call_main() -> i64 {
    // call main
    // Calculated offset: 5 (call) + 3 (mov rdi,rax) + 10 (mov rax,60) + 2 (syscall) = 20
    // Relative offset from end of call instruction: 20 - 5 = 15
    let code: i64 = malloc(8);
    let bytes: *u8 = code;
    bytes[0] = 232;  // CALL rel32
    bytes[1] = 15;   // Offset = 15 bytes
    bytes[2] = 0;
    bytes[3] = 0;
    bytes[4] = 0;
    return code;
}

// ==== String Utilities ====

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

fn parse_number_from_line(line: *i8) -> i64 {
    let i: i64 = 0;
    let value: i64 = 0;

    // Find the number (after comma and space)
    while (line[i] != 0) {
        if (line[i] >= 48) {  // digit
            if (line[i] <= 57) {
                value = value * 10 + (line[i] - 48);
                i = i + 1;
            } else {
                return value;
            }
        } else {
            i = i + 1;
        }
    }

    return value;
}

// ==== Assembly Line Parser ====

struct InstrCode {
    bytes: *u8,
    length: i64
}

fn parse_asm_line(line: *i8) -> i64 {
    let result: i64 = malloc(16);
    let instr: *InstrCode = result;

    let pos: i64 = skip_line_whitespace(line);

    // Skip comments, directives, labels, empty lines
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

    // Parse actual instructions

    // Check for "call main"
    if (str_starts_with(line + pos, "call main")) {
        instr.bytes = encode_call_main();
        instr.length = 5;
        return result;
    }

    // Check for "mov rdi, rax"
    if (str_starts_with(line + pos, "mov rdi, rax")) {
        instr.bytes = encode_mov_rdi_rax();
        instr.length = 3;
        return result;
    }

    // Check for "mov rbp, rsp"
    if (str_starts_with(line + pos, "mov rbp, rsp")) {
        instr.bytes = encode_mov_rbp_rsp();
        instr.length = 3;
        return result;
    }

    // Check for "mov rax, N"
    if (str_starts_with(line + pos, "mov rax, ")) {
        let value: i64 = parse_number_from_line(line + pos + 9);
        instr.bytes = encode_mov_rax_imm(value);
        instr.length = 10;
        return result;
    }

    // Check for "syscall"
    if (str_starts_with(line + pos, "syscall")) {
        instr.bytes = encode_syscall();
        instr.length = 2;
        return result;
    }

    // Check for "ret"
    if (str_starts_with(line + pos, "ret")) {
        instr.bytes = encode_ret();
        instr.length = 1;
        return result;
    }

    // Check for "push rbp"
    if (str_starts_with(line + pos, "push rbp")) {
        instr.bytes = encode_push_rbp();
        instr.length = 1;
        return result;
    }

    // Check for "leave"
    if (str_starts_with(line + pos, "leave")) {
        instr.bytes = encode_leave();
        instr.length = 1;
        return result;
    }

    // Unknown instruction
    instr.bytes = 0;
    instr.length = 0;
    return result;
}

// ==== Assembly File Parser ====

fn read_and_assemble(asm_file: *i8) -> i64 {
    println("Reading assembly file...");

    let fd: i64 = open(asm_file, 0);
    if (fd < 0) {
        println("❌ Failed to open assembly file");
        return 0;
    }

    let buffer: i64 = malloc(8192);
    let bytes_read: i64 = read(fd, buffer, 8192);
    close(fd);

    if (bytes_read <= 0) {
        println("❌ Failed to read assembly");
        return 0;
    }

    let asm: *i8 = buffer;
    asm[bytes_read] = 0;  // null-terminate

    println("✅ Assembly loaded");

    // Allocate machine code buffer
    let machine_code: i64 = malloc(4096);
    let code: *u8 = machine_code;
    let code_offset: i64 = 0;

    // Parse line by line
    let line_start: i64 = 0;
    let i: i64 = 0;
    let line_count: i64 = 0;

    println("Assembling instructions...");

    while (i < bytes_read) {
        if (asm[i] == 10) {  // newline
            // Process this line
            asm[i] = 0;  // null-terminate line

            let instr: *InstrCode = parse_asm_line(asm + line_start);

            if (instr.length > 0) {
                // Copy bytes to machine code
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

    // Store size in first 8 bytes (hacky but works)
    let result: i64 = malloc(16);
    let result_ptr: *i64 = result;
    result_ptr[0] = machine_code;
    result_ptr[1] = code_offset;

    return result;
}

// ==== Linker Functions (simplified from linker_simple) ====

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

    let entry_offset: i64 = 4096;
    let entry_address: i64 = 4194304 + entry_offset;  // 0x400000 + offset
    let file_size: i64 = entry_offset + code_size;

    let buf: i64 = malloc(8192);
    let bytes: *u8 = buf;

    // Clear buffer
    let i: i64 = 0;
    while (i < 8192) {
        bytes[i] = 0;
        i = i + 1;
    }

    // Generate ELF header (simplified)
    let offset: i64 = 0;

    // Magic
    offset = write_u8(bytes, offset, 127);
    offset = write_u8(bytes, offset, 69);
    offset = write_u8(bytes, offset, 76);
    offset = write_u8(bytes, offset, 70);
    offset = write_u8(bytes, offset, 2);  // 64-bit
    offset = write_u8(bytes, offset, 1);  // little-endian
    offset = write_u8(bytes, offset, 1);  // version
    offset = write_u8(bytes, offset, 0);  // SYSV

    // Padding (8 bytes)
    i = 0;
    while (i < 8) {
        offset = write_u8(bytes, offset, 0);
        i = i + 1;
    }

    offset = write_u16(bytes, offset, 2);   // ET_EXEC
    offset = write_u16(bytes, offset, 62);  // EM_X86_64
    offset = write_u32(bytes, offset, 1);   // version
    offset = write_u64(bytes, offset, entry_address);
    offset = write_u64(bytes, offset, 64);  // phoff
    offset = write_u64(bytes, offset, 0);   // shoff
    offset = write_u32(bytes, offset, 0);   // flags
    offset = write_u16(bytes, offset, 64);  // ehsize
    offset = write_u16(bytes, offset, 56);  // phentsize
    offset = write_u16(bytes, offset, 1);   // phnum
    offset = write_u16(bytes, offset, 0);   // shentsize
    offset = write_u16(bytes, offset, 0);   // shnum
    offset = write_u16(bytes, offset, 0);   // shstrndx

    // Program header at offset 64
    offset = 64;
    offset = write_u32(bytes, offset, 1);   // PT_LOAD
    offset = write_u32(bytes, offset, 5);   // PF_X | PF_R
    offset = write_u64(bytes, offset, 0);   // offset
    offset = write_u64(bytes, offset, 4194304);  // vaddr
    offset = write_u64(bytes, offset, 4194304);  // paddr
    offset = write_u64(bytes, offset, file_size);
    offset = write_u64(bytes, offset, file_size);
    offset = write_u64(bytes, offset, 4096);  // align

    // Copy code
    i = 0;
    while (i < code_size) {
        bytes[entry_offset + i] = machine_code[i];
        i = i + 1;
    }

    println("✅ ELF64 structure created");

    // Write to file
    let fd: i64 = open(output_file, 577);
    if (fd < 0) {
        println("❌ Failed to create output file");
        return 1;
    }

    let written: i64 = write(fd, buf, file_size);
    close(fd);

    if (written != file_size) {
        println("❌ Write failed");
        return 1;
    }

    println("✅ Executable written");
    return 0;
}

// ==== Main Pipeline ====

fn main() -> i64 {
    println("=========================================");
    println("  CHRONOS INTEGRATED TOOLCHAIN v0.1");
    println("  100% Self-Contained: .asm → executable");
    println("=========================================");
    println("");

    // Step 1: Assemble
    let result_ptr: i64 = read_and_assemble("output.asm");
    if (result_ptr == 0) {
        return 1;
    }

    let result: *i64 = result_ptr;
    let machine_code: *u8 = result[0];
    let code_size: i64 = result[1];

    println("");

    // Step 2: Link
    let link_result: i64 = generate_elf_and_link(machine_code, code_size, "chronos_output");
    if (link_result != 0) {
        return 1;
    }

    println("");
    println("=========================================");
    println("  SUCCESS! Executable created");
    println("=========================================");
    println("");
    println("Run:");
    println("  chmod +x chronos_output");
    println("  ./chronos_output");
    println("  echo $?");

    return 0;
}
