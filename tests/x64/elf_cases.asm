; Programas pequeños para el escritor ELF (tests/x64/test_elf.c). Cada bloque, hasta una línea `;---`, es un programa.
; Cubren el reparto de segmentos: solo .text, con .rodata, con .data, solo .bss, las cuatro
; secciones, alineaciones grandes, y cada tipo de relocación hacia cada sección.
; Solo código
section .text
global _start
_start:
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .text que cruza varias páginas (la entrada no está al inicio del texto)
section .text
f:
    times 5000 db 0x90
    ret
global _start
_start:
    call f
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .data con una relocación absoluta de 64 bits y otra de 32 bits con signo
section .data
d: db 1, 2, 3, 4, 5
e: dq 7
section .text
global _start
_start:
    mov rax, d
    mov rcx, [e]
    mov [d + 3], cl
    lea rdx, [rel e]
    mov rdi, rax
    mov rax, 60
    syscall
;---
; solo .bss (sin .data): el segmento de datos no tiene bytes en el archivo
section .bss
buf: resb 100
n: resq 1
section .text
global _start
_start:
    mov rax, buf
    mov [n], rax
    mov byte [buf + 99], 7
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .bss con alineación de 64 tras un .data de 5 bytes
section .data
d: db 1, 2, 3, 4, 5
section .bss
align 64, resb 1
big: resb 1000
section .text
global _start
_start:
    lea rax, [big]
    mov rbx, d
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .data con align 32 (bucle caliente) y un puntero a .text dentro de los datos
section .data
align 32, db 0
tab: dq f, g
fv: dd f
section .text
f:
    ret
g:
    ret
global _start
_start:
    mov rax, [tab]
    call rax
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .rodata (después del texto, en el segmento de solo lectura) con alineación 64
section .rodata
align 64, db 0
msg: db 72, 105, 0
ptrs: dq msg
section .data
w: dq msg
section .text
global _start
_start:
    mov rsi, msg
    lea rdi, [rel msg]
    mov rax, [ptrs]
    mov rbx, [w]
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; las cuatro secciones, alineaciones de 4096 en .data y .bss
section .rodata
r: db 1
section .data
align 4096, db 0
d: dq r, d
section .bss
align 4096, resb 1
b: resq 3
section .text
global _start
_start:
    mov rax, r
    mov rbx, d
    mov rcx, b
    mov [b + 8], rax
    cmp qword [d], 1
    mov rax, 60
    xor rdi, rdi
    syscall
;---
; .data de tamaño impar y justo en el borde de una página
section .text
global _start
_start:
    mov rax, d
    mov rax, 60
    xor rdi, rdi
    syscall
    times 4030 db 0x90
section .data
d: times 3 db 9
