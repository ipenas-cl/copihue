; Programs asm.c must refuse (tests/x64/run_asm.sh). Blocks are separated by ';---'.
; Each one is refused with a message; none may produce bytes.
extern puts
section .text
    call puts
;---
section .text
    times 4 nop
;---
bits 32
section .text
    ret
;---
%define N 4
section .text
    ret
;---
org 0x400000
section .text
    ret
;---
section .text start=0x401000
    ret
;---
section .rodata align=16
msg: db 1
;---
section .data
    mov rax, 1
;---
section .text
    jmp nowhere
;---
section .data
d: db 1
section .text
    jmp d
;---
section .text
    call d
;---
section .text
a: ret
a: ret
;---
section .text
.local: ret
;---
section .text
global _start
;---
section .text
f equ 1 + 2
    ret
;---
section .text
f equ 1
f equ 2
    ret
;---
section .text
f equ 1
f: ret
;---
section .text
    db "text"
;---
section .text
a:  dq a * 2
;---
section .text
    align 3
;---
section .text
    align 16, resb 1
;---
section .text
    align 0
;---
section .text
a:  jmp a + 4
;---
section .text
a:  jmp a - a
;---
section .text
    fldpi
;---
section .text
    mov rax, undefined_symbol
;---
section .text
    lea rax, [rel undefined_symbol]
;---
section .text
    mov rax, [undefined_symbol]
;---
section .text
a:  call short a
;---
section .data
    foo bar
;---
section .data
%include "x.inc"
;---
section .text
    rep jmp a
a:  ret
;---
section .text
    jmp
;---
section .text
    db
;---
section .text
    db 256
;---
section .text
    global a, b
a:  ret
b:  ret
;---
section .text
    mov rax, 1 2
;---
; jmp short that cannot reach (130 bytes)
section .text
    jmp short .e
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
    push rbx
.e: ret
;---
; M3: data directives
section .data
    db 'a'
;---
section .data
    dw label_in_a_word
label_in_a_word: db 1
;---
section .data
a:  db 1
    db a
;---
section .data
a:  dq 1
b:  dq a + b
;---
section .data
a:  dq 1
    dq 4 + a
;---
section .data
    dq 1.5
;---
section .data
    dq 18446744073709551616
;---
section .data
    dd 4294967296
;---
section .data
    dw 65536
;---
section .data
    db -129
;---
section .data
    dq 0b101
;---
section .data
    db 0x
;---
section .data
    db 1,
;---
section .data
    db ,1
;---
section .data
    dq undefined_label
;---
section .data
    dq .x
;---
section .data
a:  dq a * 8
;---
section .data
a:  dq a - a
;---
section .rodata
    mov rax, 1
;---
section .bss
    db 1
;---
section .bss
x:  dq 5
;---
section .data
    resb 4
;---
section .text
    resb 4
;---
section .bss
    resb -1
;---
section .bss
    resb nsize
;---
section .bss
    resb
;---
section .bss
    resb 4294967296
;---
section .bss
    align 8, db 1
;---
section .bss
    align 8, resb 2
;---
section .data
    align 8, resb 1
;---
section .data
    align 6
;---
section .data
    align 8192
;---
section .data
    align 8, dw 1
;---
section .data
    times -1 db 0
;---
section .data
    times x db 0
;---
section .data
    times 2 align 8
;---
section .data
    times 3
;---
section .data
    times 3 mov rax, 1
;---
section .data
    times 3 times 2 db 0
;---
section .data
    times 20000000 db 0
;---
section .data
    global
;---
section .rodata
    global nothing_here
;---
section .data
a:  db 1
a:  db 2
;---
section .bss
a:  resb 1
a:  resb 2
;---
section .data
    jmp x
x:  db 1
;---
section .text
    mov rax, [x]
section .data
x:  db 1
section .text
    mov rax, [y]
;---
section .data
    db 0XFF
