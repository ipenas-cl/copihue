; Programs for test_obj (relocatable objects, -c). Blocks are separated by a line that starts with three dashes after a semicolon.
; Each is assembled by nasm -f elf64 and by asm.c + elf_build_obj and the objects are compared.
; 1. A call, a jump and a conditional jump to other objects, and a local call
section .text
extern ext_f
extern ext_g
global fn_a
global fn_b
fn_a:
    push rbp
    mov rbp, rsp
    call ext_f
    call fn_b
    test rax, rax
    jmp ext_g
fn_b:
    ret
;---
; 2. Every way an extern's address can be written in code
extern v
global user
section .text
user:
    mov rax, [v]
    mov rax, [v + 8]
    mov [v], rax
    mov dword [v + 4], 7
    lea rax, [v]
    mov rax, v
    mov rcx, v
    lea rax, [rel v]
    mov rax, [rel v + 16]
    mov rax, [v + rcx*8]
    cmp qword [rel v], 1
    ret
;---
; 3. Externs in data: .data, .rodata and .text
extern tbl
extern fn
section .data
global vec
vec: dq tbl, fn, tbl + 8
     dd fn
     dq 5
section .rodata
rtab: dq fn
      dq tbl - 16
section .text
global start
start:
    mov rax, [rel vec]
    mov rdx, rtab
    mov rsi, jt
    ret
jt: dq fn
;---
; 4. Labels of every section, global or not, used from every section
section .data
global dg
dg: dq 1
dloc: dq 2
    dq dg
    dq dloc
section .rodata
global rg
rg: db 1, 2, 3
align 8
rl: dq rg
section .bss
global bg
bg: resb 16
bloc: resq 4
section .text
global main2
main2:
    mov rax, dg
    mov rbx, dloc
    mov rcx, rg
    mov rdx, rl
    mov rsi, bg
    mov rdi, bloc
    mov rax, [bloc + 8]
    lea rax, [rel bg]
    lea rax, [rel dloc]
    mov rax, [dg]
    call helper
    jmp done
helper:
    ret
done:
    ret
;---
; 5. Only data
section .data
global onlydata
onlydata: dq 1, 2, 3
;---
; 6. Only .bss
section .bss
global buf
buf: resb 4096
;---
; 7. Local labels, loops, a short jump over an extern call
extern ext_h
global loops
section .text
loops:
    xor rcx, rcx
.again:
    inc rcx
    cmp rcx, 10
    jb .again
    call ext_h
    je .out
    jmp .again
.out:
    ret
other:
.again:
    ret
;---
; 8. An extern that is declared and not used, and one used only in data
extern unused
extern only_in_data
section .data
global d
d: dq only_in_data
section .text
global t
t:
    ret
;---
; 9. align, sizes of symbols and relocated fields in the middle of a function
section .text
global f1
global f2
f1:
    mov rax, f2
    ret
align 16
f2:
    call f1
    ret
align 32
section .data
align 16
global d1
d1: dq f1
align 64
d2: dq f2
