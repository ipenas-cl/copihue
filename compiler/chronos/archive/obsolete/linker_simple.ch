// Chronos Linker v0.1 - Simple ELF64 Linker
// Generates ELF64 executables from machine code
// Part of Chronos v1.0 - 100% Self-Contained

// ==== ELF64 Constants ====

// ELF Magic: 0x7F 'E' 'L' 'F'
let ELF_MAGIC_0: i64 = 127;  // 0x7F
let ELF_MAGIC_1: i64 = 69;   // 'E'
let ELF_MAGIC_2: i64 = 76;   // 'L'
let ELF_MAGIC_3: i64 = 70;   // 'F'

// ELF Class
let ELFCLASS64: i64 = 2;     // 64-bit

// ELF Data
let ELFDATA2LSB: i64 = 1;    // Little-endian

// ELF Version
let EV_CURRENT: i64 = 1;

// ELF Type
let ET_EXEC: i64 = 2;        // Executable file

// ELF Machine
let EM_X86_64: i64 = 62;     // AMD x86-64

// Program Header Type
let PT_LOAD: i64 = 1;        // Loadable segment

// Program Header Flags
let PF_X: i64 = 1;           // Execute
let PF_W: i64 = 2;           // Write
let PF_R: i64 = 4;           // Read

// Entry point address
let ENTRY_POINT: i64 = 4194304;  // 0x400000

// ==== Helper Functions ====

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
    buf[offset + 5] = 0;  // Upper bytes
    buf[offset + 6] = 0;
    buf[offset + 7] = 0;
    return offset + 8;
}

// ==== ELF64 Header Generation ====

fn generate_elf_header(buf: *u8, entry: i64, phoff: i64, phnum: i64) -> i64 {
    let offset: i64 = 0;

    // EI_MAG (magic number)
    offset = write_u8(buf, offset, ELF_MAGIC_0);
    offset = write_u8(buf, offset, ELF_MAGIC_1);
    offset = write_u8(buf, offset, ELF_MAGIC_2);
    offset = write_u8(buf, offset, ELF_MAGIC_3);

    // EI_CLASS (64-bit)
    offset = write_u8(buf, offset, ELFCLASS64);

    // EI_DATA (little-endian)
    offset = write_u8(buf, offset, ELFDATA2LSB);

    // EI_VERSION
    offset = write_u8(buf, offset, EV_CURRENT);

    // EI_OSABI (SYSV = 0)
    offset = write_u8(buf, offset, 0);

    // EI_ABIVERSION
    offset = write_u8(buf, offset, 0);

    // EI_PAD (7 bytes of padding)
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);
    offset = write_u8(buf, offset, 0);

    // e_type (ET_EXEC = 2)
    offset = write_u16(buf, offset, ET_EXEC);

    // e_machine (EM_X86_64 = 62)
    offset = write_u16(buf, offset, EM_X86_64);

    // e_version
    offset = write_u32(buf, offset, EV_CURRENT);

    // e_entry (entry point address)
    offset = write_u64(buf, offset, entry);

    // e_phoff (program header offset)
    offset = write_u64(buf, offset, phoff);

    // e_shoff (section header offset - 0 for now)
    offset = write_u64(buf, offset, 0);

    // e_flags
    offset = write_u32(buf, offset, 0);

    // e_ehsize (ELF header size = 64)
    offset = write_u16(buf, offset, 64);

    // e_phentsize (program header size = 56)
    offset = write_u16(buf, offset, 56);

    // e_phnum (number of program headers)
    offset = write_u16(buf, offset, phnum);

    // e_shentsize (section header size = 0 for now)
    offset = write_u16(buf, offset, 0);

    // e_shnum (number of section headers = 0)
    offset = write_u16(buf, offset, 0);

    // e_shstrndx (section name string table index = 0)
    offset = write_u16(buf, offset, 0);

    return offset;  // Should be 64
}

// ==== Program Header Generation ====

fn generate_program_header(buf: *u8, offset: i64, file_offset: i64, vaddr: i64, filesz: i64, memsz: i64) -> i64 {
    let pos: i64 = offset;

    // p_type (PT_LOAD = 1)
    pos = write_u32(buf, pos, PT_LOAD);

    // p_flags (PF_X | PF_R = 5)
    pos = write_u32(buf, pos, PF_X + PF_R);

    // p_offset (file offset)
    pos = write_u64(buf, pos, file_offset);

    // p_vaddr (virtual address)
    pos = write_u64(buf, pos, vaddr);

    // p_paddr (physical address - same as vaddr)
    pos = write_u64(buf, pos, vaddr);

    // p_filesz (size in file)
    pos = write_u64(buf, pos, filesz);

    // p_memsz (size in memory)
    pos = write_u64(buf, pos, memsz);

    // p_align (alignment = 0x1000)
    pos = write_u64(buf, pos, 4096);

    return pos;  // Should advance by 56 bytes
}

// ==== Main Linker Function ====

fn link_executable(code: *u8, code_size: i64, output_file: *i8) -> i64 {
    println("========================================");
    println("  CHRONOS LINKER v0.1");
    println("  ELF64 Executable Generator");
    println("========================================");
    println("");

    // Calculate sizes
    let elf_header_size: i64 = 64;
    let program_header_size: i64 = 56;
    let headers_size: i64 = elf_header_size + program_header_size;

    // Entry point is right after headers
    let entry_offset: i64 = 4096;  // Page-aligned
    let entry_address: i64 = ENTRY_POINT + entry_offset;

    // Total file size
    let file_size: i64 = entry_offset + code_size;

    print("Code size: ");
    print_int(code_size);
    println(" bytes");

    print("Entry point: ");
    print_int(entry_address);
    println("");

    println("");
    println("Generating ELF64 executable...");

    // Allocate buffer for entire file
    let buf: i64 = malloc(8192);
    let bytes: *u8 = buf;

    // Clear buffer
    let i: i64 = 0;
    while (i < 8192) {
        bytes[i] = 0;
        i = i + 1;
    }

    // Generate ELF header
    let offset: i64 = generate_elf_header(bytes, entry_address, 64, 1);
    println("✅ ELF header generated (64 bytes)");

    // Generate program header
    offset = generate_program_header(bytes, 64, 0, ENTRY_POINT, file_size, file_size);
    println("✅ Program header generated (56 bytes)");

    // Copy code to entry offset
    i = 0;
    while (i < code_size) {
        bytes[entry_offset + i] = code[i];
        i = i + 1;
    }

    print("✅ Code copied (");
    print_int(code_size);
    println(" bytes)");

    // Write to file
    let fd: i64 = open(output_file, 577);  // O_WRONLY | O_CREAT | O_TRUNC
    if (fd < 0) {
        println("❌ Failed to create output file");
        return 1;
    }

    let written: i64 = write(fd, buf, file_size);
    close(fd);

    if (written != file_size) {
        println("❌ Failed to write complete file");
        return 1;
    }

    println("✅ Executable written");

    // Make executable (chmod +x)
    // syscall(chmod, filename, 0755)
    // For now, user must do: chmod +x output

    println("");
    println("========================================");
    println("  LINKING SUCCESSFUL!");
    println("========================================");
    println("");
    println("To run:");
    print("  chmod +x ");
    println(output_file);
    print("  ./");
    println(output_file);

    return 0;
}

// ==== Test Main ====

fn main() -> i64 {
    println("Testing linker with simple program...");
    println("");

    // Simple program that exits with code 42
    // mov rax, 60  ; sys_exit
    // mov rdi, 42  ; exit code
    // syscall

    let code: i64 = malloc(32);
    let bytes: *u8 = code;

    // mov rax, 60
    bytes[0] = 72;   // REX.W
    bytes[1] = 184;  // MOV rax, imm64
    bytes[2] = 60;
    bytes[3] = 0;
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 0;
    bytes[7] = 0;
    bytes[8] = 0;
    bytes[9] = 0;

    // mov rdi, 42
    bytes[10] = 72;   // REX.W
    bytes[11] = 191;  // MOV rdi, imm64
    bytes[12] = 42;
    bytes[13] = 0;
    bytes[14] = 0;
    bytes[15] = 0;
    bytes[16] = 0;
    bytes[17] = 0;
    bytes[18] = 0;
    bytes[19] = 0;

    // syscall
    bytes[20] = 15;
    bytes[21] = 5;

    let code_size: i64 = 22;

    // Link it
    link_executable(bytes, code_size, "test_program");

    return 0;
}
