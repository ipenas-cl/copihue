; Programs asm.c must refuse (tests/x64/run_asm.sh). Blocks are separated by ';---'.
; Each one is refused with a message; none may produce bytes.
extern puts
section .text
    call puts
;---
section .text
    times 4 db 0
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
section .rodata
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
a:  dq a
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
