# codegen.s - Assembly code generator
# Part of Chronos Compiler
# Generates x86-64 assembly from AST

.text

# generate_code(ast: *AST) -> output: *char
# Generates assembly code from AST
# Args: %rdi = AST pointer
# Returns: %rax = output buffer (null-terminated string)
.global generate_code
generate_code:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Debug: entered generate_code
    pushq %rdi
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_gen_enter(%rip), %rsi
    movq $15, %rdx
    syscall
    popq %rdi

    # Save AST
    movq %rdi, %r12

    # Allocate output buffer
    movq $16384, %rdi       # 16KB buffer
    call allocate_buffer
    movq %rax, %r13         # Save output buffer
    movq %rax, %r14         # Current write position

    # Debug: allocated buffer
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_gen_alloc(%rip), %rsi
    movq $15, %rdx
    syscall

    # Debug: about to read AST
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_read_ast(%rip), %rsi
    movq $15, %rdx
    syscall

    # Check AST type
    movq (%r12), %rbx       # Save AST type in %rbx

    # Debug: read AST type
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_ast_type(%rip), %rsi
    movq $14, %rdx
    syscall

    cmpq $1, %rbx           # Type == PROGRAM?
    je .gen_program

    # Unknown type - error
    jmp .gen_error

.gen_program:
    # Debug: in gen_program
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_gen_program(%rip), %rsi
    movq $17, %rdx
    syscall

    # Generate program header
    leaq asm_header(%rip), %rsi
    call append_to_output

    # Debug: appended header
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_header_done(%rip), %rsi
    movq $18, %rdx
    syscall

    # Generate stack allocation prologue
    call generate_stack_prologue

    # Generate variable initialization
    call generate_variable_init

    # Generate each action
    movq 16(%r12), %r15     # action_count

    # Debug: got action count
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_action_count(%rip), %rsi
    movq $21, %rdx
    syscall

    xorq %rbx, %rbx         # current_action = 0

.gen_actions_loop:
    # Debug: loop iteration
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_loop_iter(%rip), %rsi
    movq $17, %rdx
    syscall

    cmpq %rbx, %r15
    jle .gen_footer

    # Debug: before getting action
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_before_action(%rip), %rsi
    movq $20, %rdx
    syscall

    # Get action string
    leaq 24(%r12), %rax     # actions array
    movq (%rax, %rbx, 8), %rdi  # action[i]

    # Debug: check if pointer is NULL
    testq %rdi, %rdi
    jz .pointer_is_null

    # Debug: got action string
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_got_action(%rip), %rsi
    movq $18, %rdx
    syscall

    # Restore action string pointer
    leaq 24(%r12), %rax
    movq (%rax, %rbx, 8), %rdi

    jmp .action_ok

.pointer_is_null:
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_null_pointer(%rip), %rsi
    movq $21, %rdx
    syscall
    movq $60, %rax
    movq $1, %rdi
    syscall

.action_ok:

    # Generate print code for this action (pass index in %rsi)
    movq %rbx, %rsi
    call generate_print_action

    # Debug: returned from generate_print_action
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_after_action(%rip), %rsi
    movq $19, %rdx
    syscall

    incq %rbx
    jmp .gen_actions_loop

.gen_footer:
    # Generate stack epilogue
    call generate_stack_epilogue

    # Generate exit code
    leaq asm_exit(%rip), %rsi
    call append_to_output

    # Generate data section
    leaq asm_data_section(%rip), %rsi
    call append_to_output

    # Generate string labels and data
    movq 16(%r12), %r15     # action_count
    xorq %rbx, %rbx         # current_action = 0

.gen_data_loop:
    cmpq %rbx, %r15
    jle .gen_done

    # Get action string (possibly tagged pointer)
    leaq 24(%r12), %rax
    movq (%rax, %rbx, 8), %rdi

    # Check if this is a variable name (tagged pointer - low bit set)
    testq $1, %rdi
    jnz .skip_data_for_variable

    # It's a string literal - generate data for it
    # (pass index in %rsi)
    movq %rbx, %rsi
    call generate_string_data

.skip_data_for_variable:
    incq %rbx
    jmp .gen_data_loop

.gen_done:
    # Return output buffer
    movq %r13, %rax

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

.gen_error:
    movq $1, %rax
    movq $2, %rdi
    leaq gen_error_msg(%rip), %rsi
    movq $gen_error_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

# generate_print_action(string: *char, index: u64)
# Generates assembly for printing a string or variable
# Args: %rdi = string to print (or variable name), %rsi = string index
# Uses: %r14 (output position - modified), preserves %r12, %r13, %r15
generate_print_action:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r15

    # Debug: entered generate_print_action
    pushq %rdi
    pushq %rsi
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_print_enter(%rip), %rsi
    movq $25, %rdx
    syscall
    popq %rsi
    popq %rdi

    # Save string/variable pointer and index
    movq %rdi, %rbx
    movq %rsi, %r12

    # Check if this is a tagged pointer (low bit set = variable name)
    testq $1, %rbx
    jnz .is_variable_name

    # Low bit is 0 - it's a string literal
    jmp .print_string_literal_gen

.is_variable_name:
    # Clear the tag bit to get actual pointer
    andq $-2, %rbx

    # Look up variable in symbol table
    movq %rbx, %rdi
    call symbol_table_lookup
    testq %rax, %rax
    jz .var_not_found

    # %rax points to symbol entry
    # Generate code to load variable and print it
    call generate_print_variable
    jmp .print_done_gen

.var_not_found:
    # Error: variable not found
    pushq %rbx
    movq $1, %rax
    movq $2, %rdi
    leaq var_not_found_msg(%rip), %rsi
    movq $var_not_found_len, %rdx
    syscall
    popq %rbx

    # Print variable name
    movq $1, %rax
    movq $2, %rdi
    movq %rbx, %rsi
    movq %rbx, %rdi
    call strlen
    movq %rax, %rdx
    movq %rbx, %rsi
    movq $1, %rax
    movq $2, %rdi
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

.print_string_literal_gen:

    # Debug: about to call strlen
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_before_strlen(%rip), %rsi
    movq $23, %rdx
    syscall

    # Calculate string length
    movq %rbx, %rdi
    call strlen

    # Debug: returned from strlen
    pushq %rax
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_after_strlen(%rip), %rsi
    movq $22, %rdx
    syscall
    popq %rax

    movq %rax, %r15         # Save length

    # Check if string ends with newline - if not, add 1 to length
    decq %rax
    movzbq (%rbx, %rax), %rcx
    cmpb $'\n', %cl
    je .length_ok
    incq %r15               # Add 1 for the newline we'll add

.length_ok:

    # Debug: calculated length
    movq $1, %rax
    movq $1, %rdi
    leaq dbg_strlen_done(%rip), %rsi
    movq $21, %rdx
    syscall

    # Emit comment
    leaq print_comment(%rip), %rsi
    call append_to_output

    # Emit write syscall setup
    leaq print_code1(%rip), %rsi
    call append_to_output

    # Emit label reference (.Lstr_N)
    movb $'.', (%r14)
    incq %r14
    movb $'L', (%r14)
    incq %r14
    movb $'s', (%r14)
    incq %r14
    movb $'t', (%r14)
    incq %r14
    movb $'r', (%r14)
    incq %r14
    movb $'_', (%r14)
    incq %r14

    # Convert action index to string
    movq %r12, %rdi
    call uint_to_str_inline

    leaq print_code2(%rip), %rsi
    call append_to_output

    # Convert length to string and append
    movq %r15, %rdi
    call uint_to_str_inline

    leaq print_code3(%rip), %rsi
    call append_to_output

.print_done_gen:
    popq %r15
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# generate_string_data(string: *char, index: u64)
# Generates .data section entry for string
# Args: %rdi = string, %rsi = string index
# Uses: %r14 (output position - modified), preserves %r12, %r13, %r15
generate_string_data:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r15

    movq %rdi, %rbx         # Save string
    movq %rsi, %r12         # Save index

    # Emit label
    movb $'.', (%r14)
    incq %r14
    movb $'L', (%r14)
    incq %r14
    movb $'s', (%r14)
    incq %r14
    movb $'t', (%r14)
    incq %r14
    movb $'r', (%r14)
    incq %r14
    movb $'_', (%r14)
    incq %r14

    # String index
    movq %r12, %rdi
    call uint_to_str_inline

    movb $':', (%r14)
    incq %r14
    movb $'\n', (%r14)
    incq %r14

    # Emit .ascii directive
    leaq ascii_directive(%rip), %rsi
    call append_to_output

    # Emit string with quotes
    movb $'"', (%r14)
    incq %r14

    movq %rbx, %rsi
    call append_to_output

    # Add newline to string if not present
    movq %rbx, %rdi
    call strlen
    decq %rax
    movzbq (%rbx, %rax), %rcx
    cmpb $'\n', %cl
    je .has_newline

    movb $'\\', (%r14)
    incq %r14
    movb $'n', (%r14)
    incq %r14

.has_newline:
    movb $'"', (%r14)
    incq %r14
    movb $'\n', (%r14)
    incq %r14

    popq %r15
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# append_to_output(str: *char)
# Appends string to output buffer
# Args: %rsi = string to append
append_to_output:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rdi

.append_loop:
    movzbq (%rsi), %rax
    testb %al, %al
    jz .append_done

    movb %al, (%r14)
    incq %r14
    incq %rsi
    jmp .append_loop

.append_done:
    popq %rdi
    popq %rbp
    ret

# uint_to_str_inline(value: u64)
# Converts unsigned int to string and appends to output
# Args: %rdi = value
uint_to_str_inline:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Simple conversion (writes digits directly to output)
    # For simplicity, handle 0-999

    movq %rdi, %rax
    xorq %rdx, %rdx
    movq $10, %rcx

    # Get digits by division
    divq %rcx               # %rax = value/10, %rdx = value%10
    movq %rax, %rbx         # Save quotient

    # If quotient > 0, recursively output it
    testq %rbx, %rbx
    jz .output_digit

    # Output higher digits first
    movq %rbx, %rdi
    pushq %rdx
    call uint_to_str_inline
    popq %rdx

.output_digit:
    # Output this digit
    addb $'0', %dl
    movb %dl, (%r14)
    incq %r14

    popq %rbx
    popq %rbp
    ret

# generate_stack_prologue()
# Generates code to allocate stack space for variables
# Uses: %r14 (output position - modified)
generate_stack_prologue:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Get total stack size needed
    call symbol_table_get_stack_size
    movq %rax, %rbx         # Save stack size
    testq %rbx, %rbx
    jz .no_stack_needed

    # Emit stack allocation code
    leaq stack_prologue1(%rip), %rsi
    call append_to_output

    # Emit stack size
    movq %rbx, %rdi
    call uint_to_str_inline

    leaq stack_prologue2(%rip), %rsi
    call append_to_output

.no_stack_needed:
    popq %rbx
    popq %rbp
    ret

# generate_stack_epilogue()
# Generates code to deallocate stack space
# Uses: %r14 (output position - modified)
generate_stack_epilogue:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Get total stack size needed
    call symbol_table_get_stack_size
    movq %rax, %rbx         # Save stack size
    testq %rbx, %rbx
    jz .no_stack_to_free

    # Emit stack deallocation code
    leaq stack_epilogue1(%rip), %rsi
    call append_to_output

    # Emit stack size
    movq %rbx, %rdi
    call uint_to_str_inline

    leaq stack_epilogue2(%rip), %rsi
    call append_to_output

.no_stack_to_free:
    popq %rbx
    popq %rbp
    ret

# generate_variable_init()
# Generates code to initialize variables with their initial values
# Uses: %r14 (output position - modified)
generate_variable_init:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r15

    # Get symbol count
    call symbol_table_get_count
    movq %rax, %r15         # %r15 = symbol count
    xorq %r12, %r12         # %r12 = current index

.init_loop:
    cmpq %r15, %r12
    jge .init_done

    # Calculate symbol entry address: symbol_table + (index * 64)
    movq %r12, %rax
    movq $64, %rbx
    mulq %rbx
    leaq symbol_table(%rip), %rbx
    addq %rbx, %rax
    movq %rax, %r13         # %r13 = entry address

    # Get expression AST pointer (at offset 56)
    movq 56(%r13), %rdi
    testq %rdi, %rdi
    jz .skip_init           # No expression, skip

    # Emit comment
    leaq init_var_comment(%rip), %rsi
    call append_to_output

    # Evaluate expression and generate code
    # %rdi already has expression AST
    call evaluate_expression
    # Result is in %rax

    # Get variable's stack offset (at offset 40)
    movq 40(%r13), %rbx
    testq %rbx, %rbx
    jz .use_workaround_offset

    # Use offset from symbol table
    movq %rbx, %r8
    jmp .have_offset

.use_workaround_offset:
    # WORKAROUND: Use calculated offset from index
    # First var at -4, second at -8, etc (assuming all i32 for now)
    movq %r12, %r8
    incq %r8
    shlq $2, %r8            # multiply by 4 (i32 size)

.have_offset:
    # Store result in variable on stack
    # Generate: movq %rax, -OFFSET(%rsp)
    leaq store_var_code1(%rip), %rsi
    call append_to_output

    movq %r8, %rdi
    call uint_to_str_inline

    leaq store_var_code2(%rip), %rsi
    call append_to_output

.skip_init:
    incq %r12
    jmp .init_loop

.init_done:
    popq %r15
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# generate_print_variable(symbol_entry: *Symbol)
# Generates code to print a variable's value
# Args: %rax = pointer to symbol entry
# Uses: %r14 (output position - modified)
generate_print_variable:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13

    movq %rax, %rbx         # Save symbol entry pointer

    # WORKAROUND: Use fixed offset for first variable (4 bytes for i32)
    # TODO: Fix symbol table offset storage bug
    movq $4, %r12           # stack offset

    # Get variable's type (at offset 32 in entry)
    movl 32(%rbx), %r13d    # type

    # Emit comment
    leaq print_var_comment(%rip), %rsi
    call append_to_output

    # For simplicity, we'll convert to string and print
    # This is a simplified version - in reality we'd need proper formatting

    # Load variable from stack into %rax
    leaq load_var_code(%rip), %rsi
    call append_to_output

    # Emit stack offset
    movq %r12, %rdi
    call uint_to_str_inline

    leaq load_var_code2(%rip), %rsi
    call append_to_output

    # TODO: Convert integer to string and print
    # For now, just print a placeholder

    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# evaluate_expression(expr_node: *ExprNode)
# Generates code to evaluate an expression AST
# Args: %rdi = pointer to expression node
# Uses: %r14 (output position - modified)
# Generated code leaves result in %rax
evaluate_expression:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r15

    movq %rdi, %rbx         # Save node pointer

    # Get node op (at offset 0)
    movq 0(%rbx), %rax

    # Check if it's a number node
    cmpb $'N', %al
    je .eval_number

    # Check if it's a variable node
    cmpb $'V', %al
    je .eval_variable

    # Check operators
    cmpb $'+', %al
    je .eval_add
    cmpb $'-', %al
    je .eval_sub
    cmpb $'*', %al
    je .eval_mul
    cmpb $'/', %al
    je .eval_div

    # Unknown operator - error (for now, just return 0)
    jmp .eval_done

.eval_number:
    # Get value (at offset 8)
    movq 8(%rbx), %r12

    # Generate: movq $VALUE, %rax
    leaq eval_num_code1(%rip), %rsi
    call append_to_output

    movq %r12, %rdi
    call uint_to_str_inline

    leaq eval_num_code2(%rip), %rsi
    call append_to_output

    jmp .eval_done

.eval_variable:
    # Get variable name pointer (at offset 16)
    movq 16(%rbx), %rdi

    # Look up variable in symbol table
    pushq %rbx
    call symbol_table_lookup
    popq %rbx

    # Check if found
    testq %rax, %rax
    jz .eval_var_not_found

    # Get stack offset from symbol entry (at offset 40)
    movq 40(%rax), %r12
    testq %r12, %r12
    jnz .eval_var_has_offset

    # WORKAROUND: If offset is 0, use symbol index * 4
    # This is the same workaround as in generate_variable_init
    # For now, we'll just use 4 as a placeholder
    movq $4, %r12

.eval_var_has_offset:
    # Generate: movq -OFFSET(%rsp), %rax
    leaq eval_var_load1(%rip), %rsi
    call append_to_output

    movq %r12, %rdi
    call uint_to_str_inline

    leaq eval_var_load2(%rip), %rsi
    call append_to_output

    jmp .eval_done

.eval_var_not_found:
    # Variable not found - generate movq $0, %rax
    leaq eval_num_code1(%rip), %rsi
    call append_to_output
    movq $0, %rdi
    call uint_to_str_inline
    leaq eval_num_code2(%rip), %rsi
    call append_to_output
    jmp .eval_done

.eval_add:
    # Save operator
    movb $'+', %r15b
    jmp .eval_binop

.eval_sub:
    movb $'-', %r15b
    jmp .eval_binop

.eval_mul:
    movb $'*', %r15b
    jmp .eval_binop

.eval_div:
    movb $'/', %r15b
    jmp .eval_binop

.eval_binop:
    # Evaluate left child
    movq 16(%rbx), %rdi     # left child pointer
    call evaluate_expression

    # Push %rax onto stack (save left result)
    leaq eval_push_rax(%rip), %rsi
    call append_to_output

    # Evaluate right child
    movq 24(%rbx), %rdi     # right child pointer
    call evaluate_expression

    # Move right result to %rcx
    leaq eval_mov_to_rcx(%rip), %rsi
    call append_to_output

    # Pop left result into %rax
    leaq eval_pop_rax(%rip), %rsi
    call append_to_output

    # Perform operation based on operator
    cmpb $'+', %r15b
    je .gen_add
    cmpb $'-', %r15b
    je .gen_sub
    cmpb $'*', %r15b
    je .gen_mul
    cmpb $'/', %r15b
    je .gen_div

.gen_add:
    leaq eval_add_code(%rip), %rsi
    call append_to_output
    jmp .eval_done

.gen_sub:
    leaq eval_sub_code(%rip), %rsi
    call append_to_output
    jmp .eval_done

.gen_mul:
    leaq eval_mul_code(%rip), %rsi
    call append_to_output
    jmp .eval_done

.gen_div:
    leaq eval_div_code(%rip), %rsi
    call append_to_output
    jmp .eval_done

.eval_done:
    popq %r15
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

.data
asm_header:
    .ascii ".text\n"
    .ascii ".global _start\n"
    .ascii "\n"
    .ascii "_start:\n"
    .byte 0

asm_exit:
    .ascii "\n"
    .ascii "    # Exit\n"
    .ascii "    movq $60, %rax\n"
    .ascii "    xorq %rdi, %rdi\n"
    .ascii "    syscall\n"
    .ascii "\n"
    .byte 0

asm_data_section:
    .ascii ".data\n"
    .byte 0

print_comment:
    .ascii "    # Print\n"
    .byte 0

print_code1:
    .ascii "    movq $1, %rax\n"
    .ascii "    movq $1, %rdi\n"
    .ascii "    leaq "
    .byte 0

print_code2:
    .ascii "(%rip), %rsi\n"
    .ascii "    movq $"
    .byte 0

print_code3:
    .ascii ", %rdx\n"
    .ascii "    syscall\n"
    .ascii "\n"
    .byte 0

ascii_directive:
    .ascii "    .ascii "
    .byte 0

gen_error_msg:
    .ascii "Error: Code generation failed\n"
gen_error_len = . - gen_error_msg

var_not_found_msg:
    .ascii "Error: Variable not found: "
var_not_found_len = . - var_not_found_msg

stack_prologue1:
    .ascii "    # Allocate stack space\n"
    .ascii "    subq $"
    .byte 0

stack_prologue2:
    .ascii ", %rsp\n"
    .ascii "\n"
    .byte 0

stack_epilogue1:
    .ascii "    # Deallocate stack space\n"
    .ascii "    addq $"
    .byte 0

stack_epilogue2:
    .ascii ", %rsp\n"
    .ascii "\n"
    .byte 0

print_var_comment:
    .ascii "    # Print variable\n"
    .byte 0

load_var_code:
    .ascii "    movq -"
    .byte 0

load_var_code2:
    .ascii "(%rsp), %rax\n"
    .ascii "    # TODO: Convert %rax to string and print\n"
    .ascii "\n"
    .byte 0

dbg_gen_enter:
    .ascii "[GEN] entered\n"

dbg_gen_alloc:
    .ascii "[GEN] alloced\n"

dbg_read_ast:
    .ascii "[GEN] read AST\n"

dbg_ast_type:
    .ascii "[GEN] AST type\n"

dbg_gen_program:
    .ascii "[GEN] gen_program\n"

dbg_header_done:
    .ascii "[GEN] header done\n"

dbg_action_count:
    .ascii "[GEN] action_count OK\n"

dbg_loop_iter:
    .ascii "[GEN] loop iter\n"

dbg_before_action:
    .ascii "[GEN] before action\n"

dbg_got_action:
    .ascii "[GEN] got action\n"

dbg_null_pointer:
    .ascii "[GEN] NULL pointer!\n"

dbg_after_action:
    .ascii "[GEN] after action\n"

dbg_print_enter:
    .ascii "[PRINT] entered function\n"

dbg_before_strlen:
    .ascii "[PRINT] before strlen\n"

dbg_after_strlen:
    .ascii "[PRINT] after strlen\n"

dbg_strlen_done:
    .ascii "[PRINT] strlen done\n"

dbg_var_offset:
    .ascii "[VAR] Offset from sym: "
dbg_var_nonzero:
    .ascii "NON-ZERO!\n"
dbg_var_zero:
    .ascii "ZERO\n"

init_var_comment:
    .ascii "    # Initialize variable\n"
    .byte 0

store_var_code1:
    .ascii "    movq %rax, -"
    .byte 0

store_var_code2:
    .ascii "(%rsp)\n"
    .byte 0

eval_num_code1:
    .ascii "    movq $"
    .byte 0

eval_num_code2:
    .ascii ", %rax\n"
    .byte 0

eval_push_rax:
    .ascii "    pushq %rax\n"
    .byte 0

eval_pop_rax:
    .ascii "    popq %rax\n"
    .byte 0

eval_mov_to_rcx:
    .ascii "    movq %rax, %rcx\n"
    .byte 0

eval_add_code:
    .ascii "    addq %rcx, %rax\n"
    .byte 0

eval_sub_code:
    .ascii "    subq %rcx, %rax\n"
    .byte 0

eval_mul_code:
    .ascii "    imulq %rcx, %rax\n"
    .byte 0

eval_div_code:
    .ascii "    xorq %rdx, %rdx\n"
    .ascii "    idivq %rcx\n"
    .byte 0

eval_var_load1:
    .ascii "    movq -"
    .byte 0

eval_var_load2:
    .ascii "(%rsp), %rax\n"
    .byte 0

.bss
string_counter:
    .quad 0
