# parser.s - Template parser
# Part of Chronos Compiler
# Parses ultra-simple template format

.text

# parse_template(buffer: *u8, size: u64) -> ast: *AST
# Parses template and builds AST
# Args: %rdi = buffer, %rsi = size
# Returns: %rax = AST pointer
.global parse_template
parse_template:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Save buffer and size
    movq %rdi, %r12         # buffer
    movq %rsi, %r13         # size
    xorq %r14, %r14         # current position = 0

    # Initialize symbol table
    call symbol_table_init

    # Initialize expression system
    call expr_init

    # Allocate AST
    call allocate_ast
    movq %rax, %r15         # Save AST pointer

    # Initialize AST
    movq $0, (%r15)         # type = 0 (unknown)
    movq $0, 8(%r15)        # name = NULL
    movq $0, 16(%r15)       # action_count = 0

.parse_loop:
    # Check if we've reached end
    cmpq %r14, %r13
    jle .parse_done

    # Skip whitespace
    call skip_whitespace

    # Check if end after skipping whitespace
    cmpq %r14, %r13
    jle .parse_done

    # Try to match "Program"
    leaq program_keyword(%rip), %rsi
    call try_match_keyword
    testq %rax, %rax
    jnz .found_program

    # Try to match "Variables:"
    leaq variables_keyword(%rip), %rsi
    call try_match_keyword
    testq %rax, %rax
    jnz .found_variables

    # Try to match "Print"
    leaq print_keyword(%rip), %rsi
    call try_match_keyword
    testq %rax, %rax
    jnz .found_print

    # Unknown keyword - skip line
    call skip_to_newline
    jmp .parse_loop

.found_program:
    # Mark AST as Program type
    movq $1, (%r15)         # type = PROGRAM

    # Read program name
    call skip_whitespace
    call read_identifier
    movq %rax, 8(%r15)      # Store name

    jmp .parse_loop

.found_variables:
    # Skip the colon if present
    call skip_whitespace
    movzbq (%r12, %r14), %rax
    cmpb $':', %al
    jne .skip_variables_colon
    incq %r14

.skip_variables_colon:
    call skip_to_newline

    # Parse variable declarations (indented lines)
.parse_var_loop:
    # Check if we've reached end of input
    cmpq %r14, %r13
    jle .parse_loop

    # Check if line starts with whitespace (indentation)
    movzbq (%r12, %r14), %rax
    cmpb $' ', %al
    je .parse_one_var
    cmpb $'\t', %al
    je .parse_one_var
    # Not indented - end of Variables section
    jmp .parse_loop

.parse_one_var:
    call skip_whitespace

    # Read variable name
    call read_identifier
    movq %rax, %rbx         # Save name

    # Skip whitespace and colon
    call skip_whitespace
    movzbq (%r12, %r14), %rax
    cmpb $':', %al
    jne .var_parse_error
    incq %r14
    call skip_whitespace

    # Read type (i32, i64, bool)
    call read_identifier
    pushq %rbx              # Save variable name
    movq %rax, %rdi
    call parse_type
    movq %rax, %rcx         # Save type
    popq %rbx               # Restore variable name

    # Skip whitespace and '='
    call skip_whitespace
    movzbq (%r12, %r14), %rax
    cmpb $'=', %al
    jne .var_parse_error
    incq %r14
    call skip_whitespace

    # Parse expression (returns AST node in %rax, new position in %rbx)
    pushq %rbx              # Save variable name
    pushq %rcx              # Save type
    movq %r12, %rdi         # buffer
    movq %r14, %rsi         # position
    movq %r13, %rdx         # size
    call parse_expression
    movq %rax, %r8          # Save expression AST in %r8
    movq %rbx, %r14         # Update position
    popq %rcx               # Restore type
    popq %rbx               # Restore variable name

    # Add to symbol table
    # %rbx = name, %rcx = type, %r8 = expression AST
    pushq %r15              # Save AST pointer
    movq %rbx, %rdi         # name
    movq %rcx, %rsi         # type
    movq %r8, %rdx          # expression AST pointer
    call symbol_table_add_expr
    popq %r15

    # Skip to end of line
    call skip_to_newline

    jmp .parse_var_loop

.var_parse_error:
    # Print error and exit
    movq $1, %rax
    movq $2, %rdi
    leaq var_error_msg(%rip), %rsi
    movq $var_error_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

.found_print:
    # Read print argument (string literal or variable name)
    call skip_whitespace

    # Check if it's a string literal (starts with ")
    movzbq (%r12, %r14), %rax
    cmpb $'"', %al
    je .print_string_literal

    # Otherwise, it's a variable name
    call read_identifier

    # Tag the pointer as a variable (set low bit to 1)
    orq $1, %rax
    jmp .store_print_action

.print_string_literal:
    call read_string_literal
    # String literal pointer - low bit is 0 (no tagging needed)

.store_print_action:
    # Store in AST actions array (possibly tagged pointer)
    movq 16(%r15), %rbx     # action_count
    leaq 24(%r15), %rcx     # actions array start
    movq %rax, (%rcx, %rbx, 8)  # store string pointer (tagged or not)
    incq 16(%r15)           # increment action_count

    jmp .parse_loop

.parse_done:
    # Return AST
    movq %r15, %rax

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# Helper functions

# skip_whitespace()
# Advances %r14 past whitespace
skip_whitespace:
    pushq %rbp
    movq %rsp, %rbp

.skip_ws_loop:
    cmpq %r14, %r13
    jle .skip_ws_done

    movzbq (%r12, %r14), %rax
    cmpb $' ', %al
    je .skip_ws_inc
    cmpb $'\t', %al
    je .skip_ws_inc
    cmpb $'\r', %al
    je .skip_ws_inc
    jmp .skip_ws_done

.skip_ws_inc:
    incq %r14
    jmp .skip_ws_loop

.skip_ws_done:
    popq %rbp
    ret

# skip_to_newline()
skip_to_newline:
    pushq %rbp
    movq %rsp, %rbp

.skip_nl_loop:
    cmpq %r14, %r13
    jle .skip_nl_done

    movzbq (%r12, %r14), %rax
    incq %r14
    cmpb $'\n', %al
    jne .skip_nl_loop

.skip_nl_done:
    popq %rbp
    ret

# try_match_keyword(keyword: *char) -> matched: bool
# Args: %rsi = keyword
# Returns: %rax = 1 if matched, 0 otherwise
try_match_keyword:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    xorq %rcx, %rcx         # index = 0

.match_kw_loop:
    # Load keyword char
    movzbq (%rsi, %rcx), %rax
    testb %al, %al
    jz .match_kw_success    # End of keyword = match

    # Load buffer char
    movq %r14, %rbx
    addq %rcx, %rbx
    cmpq %rbx, %r13
    jle .match_kw_fail      # Past end = fail

    movzbq (%r12, %rbx), %rdx
    cmpb %al, %dl
    jne .match_kw_fail

    incq %rcx
    jmp .match_kw_loop

.match_kw_success:
    # Advance position past keyword
    addq %rcx, %r14
    movq $1, %rax           # Return 1 (success)
    popq %rbx
    popq %rbp
    ret

.match_kw_fail:
    xorq %rax, %rax         # Return 0 (fail)
    popq %rbx
    popq %rbp
    ret

# read_identifier() -> str: *char
# Reads an identifier (alphanumeric + underscore)
# Returns: %rax = pointer to identifier string
read_identifier:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Allocate buffer for identifier
    movq $256, %rdi
    call allocate_buffer
    movq %rax, %rbx         # Save buffer

    xorq %rcx, %rcx         # length = 0

.read_id_loop:
    cmpq %r14, %r13
    jle .read_id_done

    movzbq (%r12, %r14), %rax

    # Check if alphanumeric or underscore
    cmpb $'_', %al
    je .read_id_char
    cmpb $'a', %al
    jl .read_id_check_upper
    cmpb $'z', %al
    jle .read_id_char

.read_id_check_upper:
    cmpb $'A', %al
    jl .read_id_check_digit
    cmpb $'Z', %al
    jle .read_id_char

.read_id_check_digit:
    cmpb $'0', %al
    jl .read_id_done
    cmpb $'9', %al
    jg .read_id_done

.read_id_char:
    # Store character
    movb %al, (%rbx, %rcx)
    incq %rcx
    incq %r14
    jmp .read_id_loop

.read_id_done:
    # Null terminate
    movb $0, (%rbx, %rcx)

    movq %rbx, %rax         # Return buffer
    popq %rbx
    popq %rbp
    ret

# read_string_literal() -> str: *char
# Reads a string literal between quotes
# Returns: %rax = pointer to string (without quotes)
read_string_literal:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Find opening quote
    movzbq (%r12, %r14), %rax
    cmpb $'"', %al
    jne .read_str_error

    incq %r14               # Skip opening quote

    # Allocate buffer for string
    movq $1024, %rdi
    call allocate_buffer
    movq %rax, %rbx         # Save buffer

    xorq %rcx, %rcx         # length = 0

.read_str_loop:
    cmpq %r14, %r13
    jle .read_str_error

    movzbq (%r12, %r14), %rax
    incq %r14

    # Check for closing quote
    cmpb $'"', %al
    je .read_str_done

    # Store character
    movb %al, (%rbx, %rcx)
    incq %rcx
    jmp .read_str_loop

.read_str_done:
    # Null terminate
    movb $0, (%rbx, %rcx)

    movq %rbx, %rax         # Return buffer
    popq %rbx
    popq %rbp
    ret

.read_str_error:
    # Print error and exit
    movq $1, %rax
    movq $2, %rdi
    leaq str_error_msg(%rip), %rsi
    movq $str_error_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

# parse_type(type_str: *char) -> type_id: u64
# Converts type string to type ID
# Args: %rdi = type string
# Returns: %rax = type ID (0=i32, 1=i64, 2=bool)
parse_type:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    movq %rdi, %rbx         # type string

    # Check for "i32"
    leaq type_i32(%rip), %rsi
    movq %rbx, %rdi
    call string_compare
    testq %rax, %rax
    jnz .type_is_i32

    # Check for "i64"
    leaq type_i64(%rip), %rsi
    movq %rbx, %rdi
    call string_compare
    testq %rax, %rax
    jnz .type_is_i64

    # Check for "bool"
    leaq type_bool(%rip), %rsi
    movq %rbx, %rdi
    call string_compare
    testq %rax, %rax
    jnz .type_is_bool

    # Unknown type - default to i32
    movq $0, %rax
    jmp .type_done

.type_is_i32:
    movq $0, %rax
    jmp .type_done

.type_is_i64:
    movq $1, %rax
    jmp .type_done

.type_is_bool:
    movq $2, %rax

.type_done:
    popq %rbx
    popq %rbp
    ret

# read_integer() -> value: i64
# Reads a signed integer from current position
# Returns: %rax = integer value
read_integer:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    xorq %rax, %rax         # result = 0
    xorq %rbx, %rbx         # negative flag = 0

    # Check for negative sign
    movzbq (%r12, %r14), %rcx
    cmpb $'-', %cl
    jne .read_int_loop
    movq $1, %rbx           # negative = true
    incq %r14

.read_int_loop:
    cmpq %r14, %r13
    jle .read_int_done

    movzbq (%r12, %r14), %rcx

    # Check if digit
    cmpb $'0', %cl
    jl .read_int_done
    cmpb $'9', %cl
    jg .read_int_done

    # result = result * 10 + (char - '0')
    imulq $10, %rax
    subb $'0', %cl
    movzbq %cl, %rcx
    addq %rcx, %rax

    incq %r14
    jmp .read_int_loop

.read_int_done:
    # Apply negative if needed
    testq %rbx, %rbx
    jz .read_int_positive
    negq %rax

.read_int_positive:
    popq %rbx
    popq %rbp
    ret

# string_compare(s1: *char, s2: *char) -> equal: bool
# Compares two null-terminated strings
# Args: %rdi = s1, %rsi = s2
# Returns: %rax = 1 if equal, 0 otherwise
string_compare:
    pushq %rbp
    movq %rsp, %rbp

    xorq %rcx, %rcx         # index = 0

.strcmp_loop:
    movzbq (%rdi, %rcx), %rax
    movzbq (%rsi, %rcx), %rdx

    cmpb %al, %dl
    jne .strcmp_not_equal

    cmpb $0, %al
    je .strcmp_equal

    incq %rcx
    jmp .strcmp_loop

.strcmp_equal:
    movq $1, %rax
    popq %rbp
    ret

.strcmp_not_equal:
    xorq %rax, %rax
    popq %rbp
    ret

.data
program_keyword:
    .ascii "Program\0"

variables_keyword:
    .ascii "Variables\0"

print_keyword:
    .ascii "Print\0"

type_i32:
    .ascii "i32\0"

type_i64:
    .ascii "i64\0"

type_bool:
    .ascii "bool\0"

str_error_msg:
    .ascii "Error: Invalid string literal\n"
str_error_len = . - str_error_msg

var_error_msg:
    .ascii "Error: Invalid variable declaration\n"
var_error_len = . - var_error_msg
