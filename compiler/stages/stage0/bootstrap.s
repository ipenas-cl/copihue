; ╔═════╦═════╦═════╗
; ║ 🛡️  ║ ⚖️  ║ ⚡  ║
; ║  C  ║  E  ║  G  ║
; ╚═════╩═════╩═════╝
; ╔═════════════════╗
; ║ wcet [T∞] bound ║
; ╚═════════════════╝
;
; Author: Ignacio Peña Sepúlveda
; Date: June 25, 2025
;
; ===========================================================================
; TEMPO BOOTSTRAP STAGE 0
; ===========================================================================
; El compilador más pequeño del mundo - 500 líneas de assembly puro
; Compila un subset mínimo de Tempo suficiente para compilar stage1
; Target: Linux x86_64
; ===========================================================================

section .data
    ; --- Mensajes ---
    msg_start       db 'Tempo Bootstrap Stage 0 v0.0.1', 10, 0
    msg_reading     db 'Reading source file...', 10, 0
    msg_parsing     db 'Parsing...', 10, 0
    msg_generating  db 'Generating assembly...', 10, 0
    msg_done        db 'Done! Output written.', 10, 0
    msg_error       db 'Error: ', 0
    
    ; --- Keywords de Tempo ---
    kw_function     db 'function', 0
    kw_let          db 'let', 0
    kw_if           db 'if', 0
    kw_else         db 'else', 0
    kw_while        db 'while', 0
    kw_return       db 'return', 0
    kw_print        db 'print', 0
    
    ; --- Tablas ---
    keywords_table  dq kw_function, kw_let, kw_if, kw_else, kw_while, kw_return, kw_print, 0
    
    ; --- Buffers ---
    input_buffer    times 65536 db 0    ; 64KB para archivo fuente
    output_buffer   times 65536 db 0    ; 64KB para assembly generado
    token_buffer    times 256 db 0      ; Buffer temporal para tokens
    
    ; --- Variables globales ---
    input_ptr       dq 0
    output_ptr      dq 0
    current_token   dq 0
    token_type      dq 0
    line_number     dq 1
    
    ; --- Tipos de token ---
    TK_EOF          equ 0
    TK_KEYWORD      equ 1
    TK_IDENTIFIER   equ 2
    TK_NUMBER       equ 3
    TK_STRING       equ 4
    TK_LPAREN       equ 5
    TK_RPAREN       equ 6
    TK_LBRACE       equ 7
    TK_RBRACE       equ 8
    TK_SEMICOLON    equ 9
    TK_COMMA        equ 10
    TK_EQUALS       equ 11
    TK_PLUS         equ 12
    TK_MINUS        equ 13
    TK_MULTIPLY     equ 14
    TK_DIVIDE       equ 15

section .bss
    ; --- AST Nodes (simplified) ---
    ast_nodes       resb 32768  ; 32KB para nodos del AST
    ast_ptr         resq 1
    
    ; --- Symbol table ---
    symbols         resb 8192   ; 8KB para tabla de símbolos
    symbol_count    resq 1

section .text
global _start

_start:
    ; Imprimir banner
    mov     rsi, msg_start
    call    print_string
    
    ; Verificar argumentos
    pop     rax             ; argc
    cmp     rax, 3
    jl      .usage_error
    
    pop     rdi             ; argv[0] - programa
    pop     rdi             ; argv[1] - archivo entrada
    pop     rsi             ; argv[2] - archivo salida
    
    ; Guardar nombre archivo salida
    push    rsi
    
    ; Leer archivo fuente
    mov     rsi, msg_reading
    call    print_string
    call    read_file
    test    rax, rax
    js      .read_error
    
    ; Parsear
    mov     rsi, msg_parsing
    call    print_string
    call    parse_program
    test    rax, rax
    js      .parse_error
    
    ; Generar código
    mov     rsi, msg_generating
    call    print_string
    call    generate_code
    
    ; Escribir archivo salida
    pop     rdi             ; Recuperar nombre archivo salida
    call    write_file
    test    rax, rax
    js      .write_error
    
    ; Éxito
    mov     rsi, msg_done
    call    print_string
    
    ; Salir
    mov     rax, 60         ; sys_exit
    xor     rdi, rdi        ; status = 0
    syscall

.usage_error:
    mov     rsi, msg_error
    call    print_string
    mov     rax, 60
    mov     rdi, 1
    syscall

.read_error:
.parse_error:
.write_error:
    mov     rsi, msg_error
    call    print_string
    mov     rax, 60
    mov     rdi, 1
    syscall

; ===========================================================================
; TOKENIZER
; ===========================================================================

get_next_token:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rcx
    
    ; Limpiar buffer de token
    mov     rdi, token_buffer
    mov     rcx, 256
    xor     al, al
    rep     stosb
    
    ; Skip whitespace
.skip_whitespace:
    mov     rsi, [input_ptr]
    movzx   rax, byte [input_buffer + rsi]
    
    cmp     al, ' '
    je      .skip_ws_char
    cmp     al, 9          ; tab
    je      .skip_ws_char
    cmp     al, 13         ; CR
    je      .skip_ws_char
    cmp     al, 10         ; LF
    jne     .check_eof
    
    ; Incrementar línea
    inc     qword [line_number]
    
.skip_ws_char:
    inc     qword [input_ptr]
    jmp     .skip_whitespace
    
.check_eof:
    test    al, al
    jnz     .check_alpha
    mov     qword [token_type], TK_EOF
    jmp     .done
    
.check_alpha:
    ; Verificar si es letra
    cmp     al, 'a'
    jl      .check_upper
    cmp     al, 'z'
    jle     .read_identifier
    
.check_upper:
    cmp     al, 'A'
    jl      .check_digit
    cmp     al, 'Z'
    jle     .read_identifier
    
.check_digit:
    cmp     al, '0'
    jl      .check_symbols
    cmp     al, '9'
    jle     .read_number
    
.check_symbols:
    cmp     al, '('
    je      .token_lparen
    cmp     al, ')'
    je      .token_rparen
    cmp     al, '{'
    je      .token_lbrace
    cmp     al, '}'
    je      .token_rbrace
    cmp     al, ';'
    je      .token_semicolon
    cmp     al, ','
    je      .token_comma
    cmp     al, '='
    je      .token_equals
    cmp     al, '+'
    je      .token_plus
    cmp     al, '-'
    je      .token_minus
    cmp     al, '*'
    je      .token_multiply
    cmp     al, '/'
    je      .token_divide
    cmp     al, '"'
    je      .read_string
    
    ; Token desconocido
    jmp     .error

.read_identifier:
    mov     rdi, token_buffer
    mov     rcx, 0
    
.read_id_loop:
    mov     rsi, [input_ptr]
    movzx   rax, byte [input_buffer + rsi]
    
    ; Verificar si sigue siendo alfanumérico
    cmp     al, 'a'
    jl      .check_id_upper
    cmp     al, 'z'
    jle     .store_id_char
    
.check_id_upper:
    cmp     al, 'A'
    jl      .check_id_digit
    cmp     al, 'Z'
    jle     .store_id_char
    
.check_id_digit:
    cmp     al, '0'
    jl      .check_id_underscore
    cmp     al, '9'
    jle     .store_id_char
    
.check_id_underscore:
    cmp     al, '_'
    jne     .end_identifier
    
.store_id_char:
    mov     [rdi + rcx], al
    inc     rcx
    inc     qword [input_ptr]
    cmp     rcx, 255
    jl      .read_id_loop
    
.end_identifier:
    ; Null terminar
    mov     byte [rdi + rcx], 0
    
    ; Verificar si es keyword
    call    check_keyword
    test    rax, rax
    jz      .is_identifier
    
    mov     qword [token_type], TK_KEYWORD
    jmp     .done
    
.is_identifier:
    mov     qword [token_type], TK_IDENTIFIER
    jmp     .done

.read_number:
    mov     rdi, token_buffer
    mov     rcx, 0
    
.read_num_loop:
    mov     rsi, [input_ptr]
    movzx   rax, byte [input_buffer + rsi]
    
    cmp     al, '0'
    jl      .end_number
    cmp     al, '9'
    jg      .end_number
    
    mov     [rdi + rcx], al
    inc     rcx
    inc     qword [input_ptr]
    cmp     rcx, 255
    jl      .read_num_loop
    
.end_number:
    mov     byte [rdi + rcx], 0
    mov     qword [token_type], TK_NUMBER
    jmp     .done

.read_string:
    inc     qword [input_ptr]  ; Skip opening quote
    mov     rdi, token_buffer
    mov     rcx, 0
    
.read_str_loop:
    mov     rsi, [input_ptr]
    movzx   rax, byte [input_buffer + rsi]
    
    cmp     al, '"'
    je      .end_string
    cmp     al, 0
    je      .error
    
    mov     [rdi + rcx], al
    inc     rcx
    inc     qword [input_ptr]
    cmp     rcx, 255
    jl      .read_str_loop
    
.end_string:
    inc     qword [input_ptr]  ; Skip closing quote
    mov     byte [rdi + rcx], 0
    mov     qword [token_type], TK_STRING
    jmp     .done

.token_lparen:
    mov     qword [token_type], TK_LPAREN
    inc     qword [input_ptr]
    jmp     .done
    
.token_rparen:
    mov     qword [token_type], TK_RPAREN
    inc     qword [input_ptr]
    jmp     .done
    
.token_lbrace:
    mov     qword [token_type], TK_LBRACE
    inc     qword [input_ptr]
    jmp     .done
    
.token_rbrace:
    mov     qword [token_type], TK_RBRACE
    inc     qword [input_ptr]
    jmp     .done
    
.token_semicolon:
    mov     qword [token_type], TK_SEMICOLON
    inc     qword [input_ptr]
    jmp     .done
    
.token_comma:
    mov     qword [token_type], TK_COMMA
    inc     qword [input_ptr]
    jmp     .done
    
.token_equals:
    mov     qword [token_type], TK_EQUALS
    inc     qword [input_ptr]
    jmp     .done
    
.token_plus:
    mov     qword [token_type], TK_PLUS
    inc     qword [input_ptr]
    jmp     .done
    
.token_minus:
    mov     qword [token_type], TK_MINUS
    inc     qword [input_ptr]
    jmp     .done
    
.token_multiply:
    mov     qword [token_type], TK_MULTIPLY
    inc     qword [input_ptr]
    jmp     .done
    
.token_divide:
    mov     qword [token_type], TK_DIVIDE
    inc     qword [input_ptr]
    jmp     .done

.error:
    mov     rax, -1
    jmp     .exit
    
.done:
    xor     rax, rax
    
.exit:
    pop     rcx
    pop     rbx
    pop     rbp
    ret

; ===========================================================================
; PARSER
; ===========================================================================

parse_program:
    push    rbp
    mov     rbp, rsp
    
    ; Inicializar AST
    mov     qword [ast_ptr], 0
    
.parse_loop:
    call    get_next_token
    
    ; Check EOF
    cmp     qword [token_type], TK_EOF
    je      .done
    
    ; Check function declaration
    cmp     qword [token_type], TK_KEYWORD
    jne     .error
    
    ; Verificar que sea "function"
    mov     rsi, token_buffer
    mov     rdi, kw_function
    call    strcmp
    test    rax, rax
    jnz     .error
    
    ; Parsear función
    call    parse_function
    test    rax, rax
    js      .error
    
    jmp     .parse_loop
    
.done:
    xor     rax, rax
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbp
    ret

parse_function:
    push    rbp
    mov     rbp, rsp
    
    ; Esperamos identificador (nombre de función)
    call    get_next_token
    cmp     qword [token_type], TK_IDENTIFIER
    jne     .error
    
    ; TODO: Guardar nombre de función
    
    ; Esperamos '('
    call    get_next_token
    cmp     qword [token_type], TK_LPAREN
    jne     .error
    
    ; TODO: Parsear parámetros
    
    ; Por ahora, esperamos ')'
    call    get_next_token
    cmp     qword [token_type], TK_RPAREN
    jne     .error
    
    ; Esperamos '{'
    call    get_next_token
    cmp     qword [token_type], TK_LBRACE
    jne     .error
    
    ; Parsear cuerpo
    call    parse_block
    test    rax, rax
    js      .error
    
    xor     rax, rax
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbp
    ret

parse_block:
    push    rbp
    mov     rbp, rsp
    
.parse_statements:
    call    get_next_token
    
    ; Check '}'
    cmp     qword [token_type], TK_RBRACE
    je      .done
    
    ; TODO: Parsear diferentes tipos de statements
    ; Por ahora, solo soportamos print
    
    cmp     qword [token_type], TK_KEYWORD
    jne     .error
    
    mov     rsi, token_buffer
    mov     rdi, kw_print
    call    strcmp
    test    rax, rax
    jnz     .error
    
    call    parse_print
    test    rax, rax
    js      .error
    
    jmp     .parse_statements
    
.done:
    xor     rax, rax
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbp
    ret

parse_print:
    push    rbp
    mov     rbp, rsp
    
    ; Esperamos '('
    call    get_next_token
    cmp     qword [token_type], TK_LPAREN
    jne     .error
    
    ; Esperamos string
    call    get_next_token
    cmp     qword [token_type], TK_STRING
    jne     .error
    
    ; TODO: Generar nodo AST para print
    
    ; Esperamos ')'
    call    get_next_token
    cmp     qword [token_type], TK_RPAREN
    jne     .error
    
    ; Esperamos ';'
    call    get_next_token
    cmp     qword [token_type], TK_SEMICOLON
    jne     .error
    
    xor     rax, rax
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbp
    ret

; ===========================================================================
; CODE GENERATOR
; ===========================================================================

generate_code:
    push    rbp
    mov     rbp, rsp
    
    ; Generar header
    mov     rdi, output_buffer
    mov     rsi, asm_header
    call    strcpy
    
    ; TODO: Recorrer AST y generar código
    
    ; Por ahora, generar hello world hardcodeado
    mov     rdi, output_buffer
    call    strlen
    add     rdi, rax
    mov     rsi, asm_hello_world
    call    strcpy
    
    ; Generar footer
    mov     rdi, output_buffer
    call    strlen
    add     rdi, rax
    mov     rsi, asm_footer
    call    strcpy
    
    xor     rax, rax
    pop     rbp
    ret

; Assembly templates
asm_header:
    db 'section .data', 10
    db '    msg db "Hello, Tempo!", 10', 10
    db '    len equ $ - msg', 10, 10
    db 'section .text', 10
    db 'global _start', 10, 10
    db '_start:', 10, 0

asm_hello_world:
    db '    ; print(msg)', 10
    db '    mov rax, 1', 10
    db '    mov rdi, 1', 10
    db '    mov rsi, msg', 10
    db '    mov rdx, len', 10
    db '    syscall', 10, 10, 0

asm_footer:
    db '    ; exit(0)', 10
    db '    mov rax, 60', 10
    db '    xor rdi, rdi', 10
    db '    syscall', 10, 0

; ===========================================================================
; UTILITY FUNCTIONS
; ===========================================================================

check_keyword:
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    mov     rbx, keywords_table
    
.check_loop:
    mov     rdi, [rbx]
    test    rdi, rdi
    jz      .not_found
    
    mov     rsi, token_buffer
    call    strcmp
    test    rax, rax
    jz      .found
    
    add     rbx, 8
    jmp     .check_loop
    
.found:
    mov     rax, 1
    jmp     .exit
    
.not_found:
    xor     rax, rax
    
.exit:
    pop     rbx
    pop     rbp
    ret

strcmp:
    push    rbp
    mov     rbp, rsp
    
.compare_loop:
    movzx   rax, byte [rdi]
    movzx   rcx, byte [rsi]
    
    cmp     rax, rcx
    jne     .not_equal
    
    test    rax, rax
    jz      .equal
    
    inc     rdi
    inc     rsi
    jmp     .compare_loop
    
.equal:
    xor     rax, rax
    jmp     .exit
    
.not_equal:
    mov     rax, 1
    
.exit:
    pop     rbp
    ret

strcpy:
    push    rbp
    mov     rbp, rsp
    
.copy_loop:
    movzx   rax, byte [rsi]
    mov     [rdi], al
    
    test    rax, rax
    jz      .done
    
    inc     rdi
    inc     rsi
    jmp     .copy_loop
    
.done:
    pop     rbp
    ret

strlen:
    push    rbp
    mov     rbp, rsp
    
    xor     rax, rax
    
.count_loop:
    cmp     byte [rdi + rax], 0
    je      .done
    inc     rax
    jmp     .count_loop
    
.done:
    pop     rbp
    ret

print_string:
    push    rbp
    mov     rbp, rsp
    push    rdi
    
    mov     rdi, rsi
    call    strlen
    mov     rdx, rax        ; length
    
    mov     rax, 1          ; sys_write
    mov     rdi, 1          ; stdout
    ; rsi already has string pointer
    syscall
    
    pop     rdi
    pop     rbp
    ret

read_file:
    push    rbp
    mov     rbp, rsp
    
    ; Abrir archivo
    mov     rax, 2          ; sys_open
    mov     rsi, 0          ; O_RDONLY
    syscall
    
    test    rax, rax
    js      .error
    
    mov     rbx, rax        ; Guardar fd
    
    ; Leer archivo
    mov     rax, 0          ; sys_read
    mov     rdi, rbx        ; fd
    mov     rsi, input_buffer
    mov     rdx, 65536      ; max size
    syscall
    
    push    rax             ; Guardar bytes leídos
    
    ; Cerrar archivo
    mov     rax, 3          ; sys_close
    mov     rdi, rbx
    syscall
    
    pop     rax             ; Recuperar bytes leídos
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbp
    ret

write_file:
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    ; Crear archivo
    mov     rax, 2          ; sys_open
    mov     rsi, 0x241      ; O_CREAT | O_WRONLY | O_TRUNC
    mov     rdx, 0755       ; permissions
    syscall
    
    test    rax, rax
    js      .error
    
    mov     rbx, rax        ; Guardar fd
    
    ; Calcular longitud
    push    rdi
    mov     rdi, output_buffer
    call    strlen
    mov     rdx, rax        ; length
    pop     rdi
    
    ; Escribir archivo
    mov     rax, 1          ; sys_write
    mov     rdi, rbx        ; fd
    mov     rsi, output_buffer
    syscall
    
    push    rax             ; Guardar bytes escritos
    
    ; Cerrar archivo
    mov     rax, 3          ; sys_close
    mov     rdi, rbx
    syscall
    
    pop     rax             ; Recuperar bytes escritos
    jmp     .exit
    
.error:
    mov     rax, -1
    
.exit:
    pop     rbx
    pop     rbp
    ret

; ===========================================================================
; FIN DEL BOOTSTRAP
; ===========================================================================