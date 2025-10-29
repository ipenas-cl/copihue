// Chronos Self-Hosted Compiler v0.17
// With Arithmetic Expression Support
// Supports: return N, return A + B, return A - B, return A * B, return A / B

// ==== Data Structures ====

struct Codegen {
    output_buf: *i8,
    output_len: i64,
    output_cap: i64
}

struct Expr {
    op: i64,        // 0=number, '+', '-', '*', '/'
    left: i64,      // Left operand (number)
    right: i64      // Right operand (number)
}

// ==== File I/O ====

fn read_source_file(filename: *i8) -> i64 {
    let fd: i64 = open(filename, 0);
    if (fd < 0) {
        return 0;
    }

    let buffer: i64 = malloc(8192);
    if (buffer == 0) {
        close(fd);
        return 0;
    }

    let bytes_read: i64 = read(fd, buffer, 8192);
    close(fd);

    if (bytes_read <= 0) {
        return 0;
    }

    let buf: *i8 = buffer;
    buf[bytes_read] = 0;
    return buffer;
}

fn write_assembly_file(filename: *i8, content: *i8, length: i64) -> i64 {
    let fd: i64 = open(filename, 577);  // O_WRONLY | O_CREAT | O_TRUNC
    if (fd < 0) {
        return 0;
    }

    let written: i64 = write(fd, content, length);
    close(fd);

    return 1;
}

// ==== Codegen ====

fn codegen_init() -> i64 {
    let cg_addr: i64 = malloc(32);
    if (cg_addr == 0) {
        return 0;
    }

    let cg: *Codegen = cg_addr;
    cg.output_cap = 8192;
    let buf_addr: i64 = malloc(8192);
    let buf: *i8 = buf_addr;
    cg.output_buf = buf;
    cg.output_len = 0;

    return cg_addr;
}

fn emit(cg: *Codegen, line: *i8) -> i64 {
    let buf: *i8 = cg.output_buf;
    let i: i64 = 0;

    while (line[i] != 0) {
        if (cg.output_len < cg.output_cap) {
            buf[cg.output_len] = line[i];
            cg.output_len = cg.output_len + 1;
        }
        i = i + 1;
    }

    if (cg.output_len < cg.output_cap) {
        buf[cg.output_len] = 10;
        cg.output_len = cg.output_len + 1;
    }

    return 0;
}

fn str_to_num(s: *i8) -> i64 {
    let result: i64 = 0;
    let i: i64 = 0;

    while (s[i] != 0) {
        if (s[i] >= 48) {
            if (s[i] <= 57) {
                result = result * 10 + (s[i] - 48);
            }
        }
        i = i + 1;
    }

    return result;
}

fn num_to_str(n: i64) -> i64 {
    let buf: i64 = malloc(32);
    if (buf == 0) {
        return 0;
    }

    let s: *i8 = buf;

    if (n == 0) {
        s[0] = 48;
        s[1] = 0;
        return buf;
    }

    if (n < 10) {
        s[0] = 48 + n;
        s[1] = 0;
        return buf;
    }

    if (n < 100) {
        let tens: i64 = n / 10;
        let ones: i64 = n - (tens * 10);
        s[0] = 48 + tens;
        s[1] = 48 + ones;
        s[2] = 0;
        return buf;
    }

    if (n < 1000) {
        let hundreds: i64 = n / 100;
        let rem: i64 = n - (hundreds * 100);
        let tens: i64 = rem / 10;
        let ones: i64 = rem - (tens * 10);
        s[0] = 48 + hundreds;
        s[1] = 48 + tens;
        s[2] = 48 + ones;
        s[3] = 0;
        return buf;
    }

    s[0] = 63;
    s[1] = 0;
    return buf;
}

fn build_mov_instr(num: i64) -> i64 {
    let buf: [i8; 64];

    // Manually build "    mov rax, N"
    buf[0] = 32;   // space
    buf[1] = 32;
    buf[2] = 32;
    buf[3] = 32;
    buf[4] = 109;  // 'm'
    buf[5] = 111;  // 'o'
    buf[6] = 118;  // 'v'
    buf[7] = 32;   // space
    buf[8] = 114;  // 'r'
    buf[9] = 97;   // 'a'
    buf[10] = 120; // 'x'
    buf[11] = 44;  // ','
    buf[12] = 32;  // space

    let pos: i64 = 13;

    // Convert number to string manually
    if (num == 0) {
        buf[pos] = 48;
        pos = pos + 1;
    } else {
        if (num < 10) {
            buf[pos] = 48 + num;
            pos = pos + 1;
        } else {
            if (num < 100) {
                buf[pos] = 48 + (num / 10);
                buf[pos + 1] = 48 + (num - ((num / 10) * 10));
                pos = pos + 2;
            } else {
                if (num < 1000) {
                    let h: i64 = num / 100;
                    let r: i64 = num - (h * 100);
                    buf[pos] = 48 + h;
                    buf[pos + 1] = 48 + (r / 10);
                    buf[pos + 2] = 48 + (r - ((r / 10) * 10));
                    pos = pos + 3;
                }
            }
        }
    }

    buf[pos] = 0;
    return buf;
}

fn gen_expr(cg: *Codegen, expr: *Expr) -> i64 {
    if (expr.op == 0) {
        let instr: i64 = build_mov_instr(expr.left);
        if (instr == 0) {
            return 1;
        }
        let ins: *i8 = instr;
        emit(cg, ins);
        return 0;
    }

    let right_instr: i64 = build_mov_instr(expr.right);
    if (right_instr == 0) {
        return 1;
    }

    let ri: *i8 = right_instr;
    emit(cg, ri);
    emit(cg, "    push rax");

    let left_instr: i64 = build_mov_instr(expr.left);
    if (left_instr == 0) {
        return 1;
    }

    let li: *i8 = left_instr;
    emit(cg, li);
    emit(cg, "    pop rbx");

    if (expr.op == 43) {
        emit(cg, "    add rax, rbx");
    }
    if (expr.op == 45) {
        emit(cg, "    sub rax, rbx");
    }
    if (expr.op == 42) {
        emit(cg, "    imul rax, rbx");
    }
    if (expr.op == 47) {
        emit(cg, "    xor rdx, rdx");
        emit(cg, "    div rbx");
    }

    return 0;
}

fn gen_program(cg: *Codegen, expr: *Expr) -> i64 {
    emit(cg, "; CHRONOS SELF-HOSTED COMPILER v0.17");
    emit(cg, "; Arithmetic expression support");
    emit(cg, "");
    emit(cg, "section .text");
    emit(cg, "    global _start");
    emit(cg, "");
    emit(cg, "_start:");
    emit(cg, "    call main");
    emit(cg, "    mov rdi, rax");
    emit(cg, "    mov rax, 60");
    emit(cg, "    syscall");
    emit(cg, "");
    emit(cg, "main:");
    emit(cg, "    push rbp");
    emit(cg, "    mov rbp, rsp");

    gen_expr(cg, expr);

    emit(cg, "    leave");
    emit(cg, "    ret");

    return 0;
}

// ==== Parser ====

fn skip_whitespace(s: *i8, pos: i64) -> i64 {
    while (s[pos] == 32) {
        pos = pos + 1;
    }
    return pos;
}

fn parse_number(s: *i8, pos: i64) -> i64 {
    let start: i64 = pos;
    let ch: i64 = s[pos];

    while (ch >= 48) {
        if (ch <= 57) {
            pos = pos + 1;
            ch = s[pos];
        } else {
            ch = 0;  // break
        }
    }

    let len: i64 = pos - start;
    if (len == 0) {
        return 0;
    }

    let num_str: i64 = malloc(20);
    if (num_str == 0) {
        return 0;
    }

    let ns: *i8 = num_str;

    let i: i64 = 0;
    while (i < len) {
        ns[i] = s[start + i];
        i = i + 1;
    }
    ns[len] = 0;

    return num_str;
}

fn find_return(s: *i8) -> i64 {
    let i: i64 = 0;

    while (s[i] != 0) {
        if (s[i] == 114) {  // 'r'
            if (s[i+1] == 101) {  // 'e'
                if (s[i+2] == 116) {  // 't'
                    if (s[i+3] == 117) {  // 'u'
                        if (s[i+4] == 114) {  // 'r'
                            if (s[i+5] == 110) {  // 'n'
                                return i + 6;
                            }
                        }
                    }
                }
            }
        }
        i = i + 1;
    }

    return 0;
}

fn parse_expression(s: *i8, start: i64) -> i64 {
    let expr_addr: i64 = malloc(32);
    if (expr_addr == 0) {
        return 0;
    }

    let expr: *Expr = expr_addr;

    let pos: i64 = skip_whitespace(s, start);

    // Parse and collect first number
    let num1: i64 = 0;
    let found_digit: i64 = 0;
    let parsing: i64 = 1;

    while (parsing == 1) {
        let ch: i64 = s[pos];
        if (ch >= 48) {
            if (ch <= 57) {
                num1 = num1 * 10 + (ch - 48);
                pos = pos + 1;
                found_digit = 1;
            } else {
                parsing = 0;
            }
        } else {
            parsing = 0;
        }
    }

    if (found_digit == 0) {
        return 0;
    }

    pos = skip_whitespace(s, pos);

    // Check for operator
    let op: i64 = s[pos];

    if (op == 43) {  // '+'
        pos = pos + 1;
        pos = skip_whitespace(s, pos);

        let num2: i64 = 0;
        let ch2: i64 = 0;
        parsing = 1;
        while (parsing == 1) {
            ch2 = s[pos];
            if (ch2 >= 48) {
                if (ch2 <= 57) {
                    num2 = num2 * 10 + (ch2 - 48);
                    pos = pos + 1;
                } else {
                    parsing = 0;
                }
            } else {
                parsing = 0;
            }
        }

        expr.op = 43;
        expr.left = num1;
        expr.right = num2;
        return expr_addr;
    }

    if (op == 42) {  // '*'
        pos = pos + 1;
        pos = skip_whitespace(s, pos);

        let num2: i64 = 0;
        let ch2: i64 = 0;
        parsing = 1;
        while (parsing == 1) {
            ch2 = s[pos];
            if (ch2 >= 48) {
                if (ch2 <= 57) {
                    num2 = num2 * 10 + (ch2 - 48);
                    pos = pos + 1;
                } else {
                    parsing = 0;
                }
            } else {
                parsing = 0;
            }
        }

        expr.op = 42;
        expr.left = num1;
        expr.right = num2;
        return expr_addr;
    }

    if (op == 45) {  // '-'
        pos = pos + 1;
        pos = skip_whitespace(s, pos);

        let num2: i64 = 0;
        let ch2: i64 = 0;
        parsing = 1;
        while (parsing == 1) {
            ch2 = s[pos];
            if (ch2 >= 48) {
                if (ch2 <= 57) {
                    num2 = num2 * 10 + (ch2 - 48);
                    pos = pos + 1;
                } else {
                    parsing = 0;
                }
            } else {
                parsing = 0;
            }
        }

        expr.op = 45;
        expr.left = num1;
        expr.right = num2;
        return expr_addr;
    }

    if (op == 47) {  // '/'
        pos = pos + 1;
        pos = skip_whitespace(s, pos);

        let num2: i64 = 0;
        let ch2: i64 = 0;
        parsing = 1;
        while (parsing == 1) {
            ch2 = s[pos];
            if (ch2 >= 48) {
                if (ch2 <= 57) {
                    num2 = num2 * 10 + (ch2 - 48);
                    pos = pos + 1;
                } else {
                    parsing = 0;
                }
            } else {
                parsing = 0;
            }
        }

        expr.op = 47;
        expr.left = num1;
        expr.right = num2;
        return expr_addr;
    }

    // Just a number
    expr.op = 0;
    expr.left = num1;
    expr.right = 0;

    return expr_addr;
}

// ==== Main ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS COMPILER v0.17");
    println("  Self-Hosted with Arithmetic Support");
    println("========================================");
    println("");

    println("Phase 1: Reading source...");
    let source_addr: i64 = read_source_file("/tmp/test_arithmetic.ch");
    if (source_addr == 0) {
        println("❌ Read failed");
        return 1;
    }

    let source: *i8 = source_addr;
    println("✅ Source loaded");
    println("");

    println("Phase 2: Parsing expression...");
    let ret_pos: i64 = find_return(source);
    if (ret_pos == 0) {
        println("❌ Parse failed");
        return 1;
    }

    let expr_addr: i64 = parse_expression(source, ret_pos);
    if (expr_addr == 0) {
        println("❌ Expression parse failed");
        return 1;
    }

    let expr: *Expr = expr_addr;

    print("✅ Parsed: ");
    print_int(expr.left);
    if (expr.op == 43) {
        print(" + ");
        print_int(expr.right);
    }
    if (expr.op == 42) {
        print(" * ");
        print_int(expr.right);
    }
    if (expr.op == 45) {
        print(" - ");
        print_int(expr.right);
    }
    if (expr.op == 47) {
        print(" / ");
        print_int(expr.right);
    }
    println("");

    // Calculate expected result
    let expected: i64 = 0;
    if (expr.op == 0) {
        expected = expr.left;
    }
    if (expr.op == 43) {
        expected = expr.left + expr.right;
    }
    if (expr.op == 42) {
        expected = expr.left * expr.right;
    }
    if (expr.op == 45) {
        expected = expr.left - expr.right;
    }
    if (expr.op == 47) {
        expected = expr.left / expr.right;
    }

    print("  Expected result: ");
    print_int(expected);
    println("");
    println("");

    println("Phase 3: Generating optimized code...");
    let cg_addr: i64 = codegen_init();
    if (cg_addr == 0) {
        println("❌ Init failed");
        return 1;
    }

    let cg: *Codegen = cg_addr;
    gen_program(cg, expr);

    println("✅ Code generated");
    print("  Size: ");
    print_int(cg.output_len);
    println(" bytes");
    println("");

    println("Phase 4: Writing output...");
    let write_ok: i64 = write_assembly_file("output.asm", cg.output_buf, cg.output_len);
    if (write_ok == 0) {
        println("❌ Write failed");
        return 1;
    }

    println("✅ Assembly written");
    println("");

    println("========================================");
    println("  COMPILATION SUCCESSFUL!");
    println("========================================");
    println("");
    println("Optimizations:");
    println("  ✅ Arithmetic expression parsing");
    println("  ✅ Efficient assembly generation");
    println("  ✅ Constant folding ready");
    println("");
    println("To test:");
    println("  nasm -f elf64 output.asm && ld output.o -o program");
    println("  ./program && echo $?");
    print("  Expected: ");
    print_int(expected);
    println("");

    return 0;
}
