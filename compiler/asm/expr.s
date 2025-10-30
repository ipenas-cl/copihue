# expr.s - Expression AST and Parser
# Part of Chronos Compiler - FASE 2
# Handles arithmetic expressions: a + b * 2, etc.

.section .bss
    .align 8

# Expression node pool (max 100 nodes)
# Each node: 32 bytes
#   Offset 0-7:   op ('+', '-', '*', '/', 'N' for number)
#   Offset 8-15:  value (if op == 'N')
#   Offset 16-23: left child pointer
#   Offset 24-31: right child pointer
expr_nodes:
    .skip 3200      # 100 nodes * 32 bytes

.section .data
    .align 8

expr_node_count:
    .quad 0

.section .text
.global expr_init
.global expr_new_node
.global expr_new_number
.global expr_new_variable
.global expr_new_binop

# Initialize expression system
expr_init:
    pushq %rbp
    movq %rsp, %rbp

    movq $0, expr_node_count(%rip)

    movq %rbp, %rsp
    popq %rbp
    ret

# Allocate a new expression node
# Returns: %rax = pointer to node
expr_new_node:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Get current node count
    movq expr_node_count(%rip), %rax

    # Calculate address: expr_nodes + (count * 32)
    movq $32, %rbx
    mulq %rbx
    leaq expr_nodes(%rip), %rbx
    addq %rbx, %rax

    # Save node address
    pushq %rax

    # Increment count
    movq expr_node_count(%rip), %rbx
    incq %rbx
    movq %rbx, expr_node_count(%rip)

    # Return node address
    popq %rax

    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Create a number node
# Parameters:
#   %rdi = number value
# Returns:
#   %rax = pointer to node
expr_new_number:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %rdi

    # Allocate node
    call expr_new_node
    movq %rax, %rbx     # Save node pointer

    # Set op = 'N'
    movq $'N', 0(%rbx)

    # Set value
    popq %rdi
    movq %rdi, 8(%rbx)

    # Set children to NULL
    movq $0, 16(%rbx)
    movq $0, 24(%rbx)

    movq %rbx, %rax     # Return node pointer

    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Create a variable node
# Parameters:
#   %rdi = pointer to variable name string
# Returns:
#   %rax = pointer to node
expr_new_variable:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %rdi

    # Allocate node
    call expr_new_node
    movq %rax, %rbx     # Save node pointer

    # Set op = 'V'
    movq $'V', 0(%rbx)

    # Set value = 0 (will be filled by codegen with stack offset)
    movq $0, 8(%rbx)

    # Set left child = variable name pointer
    popq %rdi
    movq %rdi, 16(%rbx)

    # Set right child to NULL
    movq $0, 24(%rbx)

    movq %rbx, %rax     # Return node pointer

    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Create a binary operation node
# Parameters:
#   %rdi = op ('+', '-', '*', '/')
#   %rsi = left child pointer
#   %rdx = right child pointer
# Returns:
#   %rax = pointer to node
expr_new_binop:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14

    # Save parameters
    movq %rdi, %r12     # op
    movq %rsi, %r13     # left
    movq %rdx, %r14     # right

    # Allocate node
    call expr_new_node
    movq %rax, %rbx     # Save node pointer

    # Set op
    movq %r12, 0(%rbx)

    # Set value = 0 (not used for binop)
    movq $0, 8(%rbx)

    # Set children
    movq %r13, 16(%rbx)
    movq %r14, 24(%rbx)

    movq %rbx, %rax     # Return node pointer

    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Parse expression with precedence climbing
# Parameters:
#   %rdi = buffer pointer
#   %rsi = start position
#   %rdx = buffer size
# Returns:
#   %rax = expression node pointer
#   %rbx = new position after expression
.global parse_expression
parse_expression:
    pushq %rbp
    movq %rsp, %rbp
    pushq %r12
    pushq %r13
    pushq %r14

    movq %rdi, %r12     # buffer
    movq %rsi, %r13     # position
    movq %rdx, %r14     # size

    # Call parse_expr_prec with min_precedence = 0
    movq $0, %rdi       # min_prec = 0
    call parse_expr_prec

    # %rax has result, %r13 has position
    movq %r13, %rbx     # Return position

    popq %r14
    popq %r13
    popq %r12
    movq %rbp, %rsp
    popq %rbp
    ret

# Parse expression with precedence climbing (recursive)
# Parameters:
#   %rdi = minimum precedence
# Returns:
#   %rax = expression node pointer
#   %r13 = updated position (modified)
parse_expr_prec:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r15
    pushq %rdi          # Save min_prec

    # Parse primary (left operand)
    call parse_primary
    movq %rax, %r15     # Save left in %r15

.prec_loop:
    # Skip whitespace
    call skip_expr_whitespace

    # Check if we have an operator
    cmpq %r13, %r14
    jge .check_operator
    jmp .prec_done

.check_operator:
    movzbq (%r12, %r13), %rax

    # Check if it's an operator
    movq %rax, %rdi
    call is_operator
    testq %rax, %rax
    jz .prec_done       # Not an operator

    # Get operator
    movzbq (%r12, %r13), %rbx   # operator in %bl

    # Get precedence
    movq %rbx, %rdi
    call get_precedence
    movq %rax, %rcx     # op_prec in %rcx

    # Compare with min_prec
    movq 0(%rsp), %rax  # Load min_prec from stack
    cmpq %rax, %rcx
    jl .prec_done       # op_prec < min_prec, done

    # Consume operator
    incq %r13

    # Skip whitespace
    call skip_expr_whitespace

    # Parse right with higher precedence
    # right = parse_expr_prec(op_prec + 1)
    movq %rcx, %rdi
    incq %rdi           # min_prec = op_prec + 1
    pushq %rbx          # Save operator
    pushq %r15          # Save left
    call parse_expr_prec
    movq %rax, %rdx     # right in %rdx
    popq %r15           # Restore left
    popq %rbx           # Restore operator

    # Create binop: left = BinOp(op, left, right)
    movq %rbx, %rdi     # operator
    movq %r15, %rsi     # left
    # %rdx already has right
    call expr_new_binop
    movq %rax, %r15     # Update left

    jmp .prec_loop

.prec_done:
    movq %r15, %rax     # Return left
    popq %rdi           # Clean up min_prec from stack
    popq %r15
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# get_precedence(op: char) -> precedence: i64
# Returns precedence of an operator
# Args: %rdi = operator character
# Returns: %rax = precedence (1 for +-, 2 for */)
get_precedence:
    pushq %rbp
    movq %rsp, %rbp

    cmpb $'*', %dil
    je .prec_2
    cmpb $'/', %dil
    je .prec_2

    cmpb $'+', %dil
    je .prec_1
    cmpb $'-', %dil
    je .prec_1

    # Default: 0
    xorq %rax, %rax
    jmp .get_prec_done

.prec_1:
    movq $1, %rax
    jmp .get_prec_done

.prec_2:
    movq $2, %rax

.get_prec_done:
    movq %rbp, %rsp
    popq %rbp
    ret

# is_operator(ch: char) -> bool
# Checks if character is an operator
# Args: %rdi = character
# Returns: %rax = 1 if operator, 0 otherwise
is_operator:
    pushq %rbp
    movq %rsp, %rbp

    cmpb $'+', %dil
    je .is_op
    cmpb $'-', %dil
    je .is_op
    cmpb $'*', %dil
    je .is_op
    cmpb $'/', %dil
    je .is_op

    xorq %rax, %rax
    jmp .is_op_done

.is_op:
    movq $1, %rax

.is_op_done:
    movq %rbp, %rsp
    popq %rbp
    ret

# parse_primary() -> expr_node
# Parses a primary expression: number, variable, or (expr)
# Uses: %r12 (buffer), %r13 (position), %r14 (size)
# Returns: %rax = expression node pointer
parse_primary:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Skip whitespace
    call skip_expr_whitespace

    # Check for '('
    cmpq %r13, %r14
    jle .primary_error
    movzbq (%r12, %r13), %rax
    cmpb $'(', %al
    je .parse_paren

    # Check if digit or '-' (negative number)
    cmpb $'-', %al
    je .parse_number_primary
    cmpb $'0', %al
    jl .parse_variable_primary
    cmpb $'9', %al
    jle .parse_number_primary

    # Otherwise, must be variable (identifier)
    jmp .parse_variable_primary

.parse_paren:
    # Skip '('
    incq %r13

    # Parse expression recursively
    movq $0, %rdi       # min_prec = 0
    call parse_expr_prec
    movq %rax, %rbx     # Save result

    # Skip whitespace
    call skip_expr_whitespace

    # Expect ')'
    cmpq %r13, %r14
    jle .primary_error
    movzbq (%r12, %r13), %rax
    cmpb $')', %al
    jne .primary_error

    # Skip ')'
    incq %r13

    movq %rbx, %rax     # Return result
    jmp .primary_done

.parse_number_primary:
    call parse_expr_number
    jmp .primary_done

.parse_variable_primary:
    call parse_identifier
    # %rax has variable name pointer
    movq %rax, %rdi
    call expr_new_variable
    jmp .primary_done

.primary_error:
    # Return NULL on error
    xorq %rax, %rax

.primary_done:
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# parse_identifier() -> str: *char
# Parses an identifier (variable name)
# Uses: %r12 (buffer), %r13 (position), %r14 (size)
# Returns: %rax = pointer to identifier string
parse_identifier:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r15

    # Allocate buffer for identifier (256 bytes)
    movq $256, %rdi
    call allocate_buffer
    movq %rax, %rbx     # Save buffer pointer

    xorq %r15, %r15     # length = 0

.ident_loop:
    cmpq %r13, %r14
    jle .ident_done

    movzbq (%r12, %r13), %rax

    # Check if alphanumeric or underscore
    cmpb $'_', %al
    je .ident_char
    cmpb $'a', %al
    jl .ident_check_upper
    cmpb $'z', %al
    jle .ident_char

.ident_check_upper:
    cmpb $'A', %al
    jl .ident_check_digit
    cmpb $'Z', %al
    jle .ident_char

.ident_check_digit:
    cmpb $'0', %al
    jl .ident_done
    cmpb $'9', %al
    jg .ident_done

.ident_char:
    # Store character
    movb %al, (%rbx, %r15)
    incq %r15
    incq %r13
    jmp .ident_loop

.ident_done:
    # Null terminate
    movb $0, (%rbx, %r15)

    movq %rbx, %rax     # Return buffer

    popq %r15
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Helper: Skip whitespace in expression
skip_expr_whitespace:
    pushq %rbp
    movq %rsp, %rbp

.skip_expr_ws_loop:
    cmpq %r13, %r14
    jle .skip_expr_ws_done

    movzbq (%r12, %r13), %rax
    cmpb $' ', %al
    je .skip_expr_ws_inc
    cmpb $'\t', %al
    je .skip_expr_ws_inc
    jmp .skip_expr_ws_done

.skip_expr_ws_inc:
    incq %r13
    jmp .skip_expr_ws_loop

.skip_expr_ws_done:
    movq %rbp, %rsp
    popq %rbp
    ret

# Helper: Parse a number from expression
# Returns: %rax = number value, %r13 = new position
parse_expr_number:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    xorq %rax, %rax     # result = 0
    xorq %rbx, %rbx     # negative flag = 0

    # Check for negative sign
    cmpq %r13, %r14
    jge .check_negative
    jmp .expr_num_done

.check_negative:
    movzbq (%r12, %r13), %rcx
    cmpb $'-', %cl
    jne .parse_expr_num_loop
    movq $1, %rbx       # negative = true
    incq %r13

.parse_expr_num_loop:
    cmpq %r13, %r14
    jle .expr_num_done

    movzbq (%r12, %r13), %rcx

    # Check if digit
    cmpb $'0', %cl
    jl .expr_num_done
    cmpb $'9', %cl
    jg .expr_num_done

    # result = result * 10 + (char - '0')
    imulq $10, %rax
    subb $'0', %cl
    movzbq %cl, %rcx
    addq %rcx, %rax

    incq %r13
    jmp .parse_expr_num_loop

.expr_num_done:
    # Apply negative if needed
    testq %rbx, %rbx
    jz .expr_num_positive
    negq %rax

.expr_num_positive:
    # Create number node
    pushq %rax
    movq %rax, %rdi
    call expr_new_number
    popq %rcx          # Discard saved value

    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret
