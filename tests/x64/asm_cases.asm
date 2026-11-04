; Programs for tests/x64/run_asm.sh: each block between ';---' lines is assembled by
; asm.c and by nasm -f elf64, and .text and relocations must be identical.
; Branch limits use 1-byte pushes, so N pushes are N bytes.
;---
; forward jmp and jne over 125 bytes (rel8 holds up to 127)
section .text
_start:
    jmp .end
    jne .end
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
.end:
    ret
;---
; forward jmp and jne over 126 bytes (rel8 holds up to 127)
section .text
_start:
    jmp .end
    jne .end
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
.end:
    ret
;---
; forward jmp and jne over 127 bytes (rel8 holds up to 127)
section .text
_start:
    jmp .end
    jne .end
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
.end:
    ret
;---
; forward jmp and jne over 128 bytes (rel8 holds up to 127)
section .text
_start:
    jmp .end
    jne .end
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
.end:
    ret
;---
; forward jmp and jne over 129 bytes (rel8 holds up to 127)
section .text
_start:
    jmp .end
    jne .end
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
.end:
    ret
;---
; backward jz and jmp over 124 bytes
section .text
_start:
.top:
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
    jz .top
    jmp .top
    ret
;---
; backward jz and jmp over 125 bytes
section .text
_start:
.top:
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
    jz .top
    jmp .top
    ret
;---
; backward jz and jmp over 126 bytes
section .text
_start:
.top:
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
    jz .top
    jmp .top
    ret
;---
; backward jz and jmp over 127 bytes
section .text
_start:
.top:
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
    jz .top
    jmp .top
    ret
;---
; backward jz and jmp over 128 bytes
section .text
_start:
.top:
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
    jz .top
    jmp .top
    ret
;---
; relaxation cascade: growing the inner jne pushes the outer jmp over the edge
section .text
_start:
    jmp .end
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
    jne .far
    push rbx
    push rbx
    push rbx
    push rbx
.end:
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
.far:
    ret
;---
; two-level cascade, backward and forward
section .text
f:
.a:
    jmp .c
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
.b:
    jz .a
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
    jnz .far
.c:
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
.far:
    jmp .b
    ret
;---
; call forward and backward, locals with the same name in two functions
section .text
global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall
helper:
.loop:
    dec rdi
    jnz .loop
    ret
main:
.loop:
    call helper
    call later
    cmp rax, 3
    jl .loop
    ret
later:
    call helper
    ret
;---
; label and instruction on one line, label alone, comments
section .text
a: ret    ; comment
b:
c: jmp a
   jmp b ; backward
;---
; align with nops and with a fill byte, in .text
section .text
_start:
    push rbx
    jmp .l
align 16
.l:
    push rbx
    align 32
.m:
    jmp .l
    align 8, db 0
    align 4, db 0xCC
    align 1
    ret
;---
; align padding changes the distance of a jump over it
section .text
_start:
    jmp .over
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
    align 32
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
.over:
    ret
;---
; equ defined after use shrinks or not the immediate; the frame idiom
section .text
f:
    push rbp
    mov rbp, rsp
    sub rsp, __frame_f
    mov [rbp - __frame_f + 8], rax
    mov rax, [rbp - __frame_f + 16]
    leave
    ret
__frame_f equ 400
g:
    sub rsp, __frame_g
    ret
__frame_g equ 64
;---
; every condition code, forward, as nasm spells them
section .text
_start:
    jz .t
    jnz .t
    je .t
    jne .t
    jl .t
    jle .t
    jg .t
    jge .t
    js .t
    jns .t
    ja .t
    jae .t
    jb .t
    jbe .t
    jc .t
    jnc .t
    jo .t
    jno .t
    jp .t
    jnp .t
    jnae .t
    jnb .t
    jna .t
    jnbe .t
    jng .t
    jnl .t
    jnge .t
    jnle .t
.t:
    ret
;---
; short and near keywords
section .text
_start:
    jmp short .a
    jmp near .a
    jne short .a
    jne near .a
.a:
    ret
;---
; rip-relative to .text, absolute reference to .text (local and global), data symbols
section .data
d: db 1, 2, 3
e: dq 5
section .bss
buf: resb 64
section .text
global _start
_start:
    lea rax, [rel c]
    movdqu xmm2, [rel c]
    mov rax, _start
    mov rbx, f
    mov rcx, d
    mov rdx, [e]
    lea rsi, [buf+8]
    mov byte [buf+rax*1], 1
    mov qword [e+8], 5
    call f
    jmp c
    align 16
c:  db 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
f:  ret
;---
; jump into a table of bytes and back; negative db
section .text
_start:
    jmp .t
.b: db -1, 255, 0x7f
.t: jmp .b
;---
; data-only sections around an empty .text; align in .data is not encoded
section .data
align 64, db 0
x: dd 1
section .bss
align 64, resb 1
y: resq 4
section .text
    ret
;---
; M3: every section, alignment inside data, symbols from .text to data (addends), global data
global _start
global gd
section .text
_start:
    mov rax, d1
    lea rbx, [rel d2]
    mov rcx, [d2+8]
    mov rdx, gd
    mov rsi, r1
    mov qword [rel d2+4], 7
    lea rdi, [b1+rax*8+16]
    mov dword [gd], 1
section .data
d1: db 1,2,3
    align 4
d2: dq 5, d1, _start, d1+3
gd: dd 1
    align 8
    dw 7
    align 16, db 0xAA
section .rodata
r1: db 9
    align 8
r2: dq d2
section .bss
b1: resb 3
    align 8
b2: resq 2
    resw 1
    align 64, resb 1
b3: resd 1
;---
; M3: number forms, 64-bit limits, label arithmetic, times, a section entered twice
section .text
_start:
    align 32
    mov rax, [rel q]
    mov rbx, [q+8]
    lea rcx, [rel q+16]
    cmp qword [rel q], 1
    mov dword [rel q+4], 1
    ret
section .data
q: dq 0xFFFFFFFFFFFFFFFF, -1, 0x7FFFFFFFFFFFFFFF, 0x10, 010, 9223372036854775808
   dd -1, 4294967295, q, q+4, 0xDEADBEEF
   dw 65535, -32768
   db 255, -128, 0x41, 010
   dq q+8, _start+3, q-2
   times 3 db 7
   times 2 dd 1, 2
   times 0 db 1
   times 2 dq q
section .rodata
   dq 1
section .bss
z: resb 0
   resq 1
   times 3 resw 2
section .data
   db 5
;---
; M3: forward and backward constants in data, equ as count and as value
K equ 7
section .data
a: db K, K+1, K-2, LATER
   dd LATER+1
   dq K
section .text
_start:
    mov eax, LATER
    ret
LATER equ 200
section .bss
buf: resb 16
;---
; M3: local labels in data belong to the last global label; sections of one label name
section .text
_start:
    mov rax, tab.second
    mov rbx, [tab.first]
    jmp .t
.t: lea rcx, [rel other.second]
    ret
section .data
tab: db 0
.first: dq 11
.second: dq tab.first, .first, _start, _start.t
other: dq 1
.second: dq other, .second
;---
; M3: a jump table in .data whose entries move when jumps relax (forward and backward targets)
section .text
_start:
    mov rax, [tbl]
    jmp f
    times 130 db 0x90
f:  jz g
    times 140 db 0x90
g:  ret
h:  jmp g
section .data
tbl: dq f, g, h, _start, g+1, h-1
     dd g, f
;---
; M3: labels at the very end of a section, empty .data, .bss only
section .text
_start:
    mov rax, end_data
    mov rbx, end_bss
    mov rcx, [end_ro]
    ret
section .data
x:  db 1, 2
end_data:
section .rodata
y:  dq 1
end_ro:
section .bss
z:  resq 3
end_bss:
;---
; M3: only .data, nothing in .text
section .data
v:  dq v, v+8, w
w:  db 65
;---
; M3: align forms in every section, including the default fill and align 1
section .text
    align 1
    nop
    align 8
    align 8, db 0xCC
    ret
section .data
    db 1
    align 2
    db 2
    align 4, db 0
    db 3
    align 16
    dd 1
    align 32, db 7
section .rodata
    db 1
    align 8
    dq 5
    align 64
section .bss
    resb 1
    align 2
    resb 1
    align 8, resb 1
    align 16
    resq 2
    align 4096, resb 1
q:  resb 1
;---
; M3: the label before and after align; label on the same line as a directive
section .data
a:  align 8
b:  db 1
c:  align 8, db 0
d:  dq 1
section .bss
e:  align 16
f:  resb 4
g:  align 16, resb 1
h:  resd 1
;---
; M3: addends of every symbol operand form against data (PC32 with an immediate after, scaled index)
section .text
_start:
    lea rax, [rel d]
    lea rax, [rel d+24]
    mov qword [rel d+8], 1
    mov byte [rel d+9], 2
    mov dword [rel d], 0x1234
    cmp byte [rel d+1], 3
    movdqu xmm2, [rel d]
    mov rax, [d]
    mov rax, [d+8]
    mov rax, [d+rbx*8]
    mov rax, [d+rbx*8+16]
    mov rax, [rbx*8+d]
    mov rax, [rbx*8+d+4]
    lea rax, [d+rbx*4+rcx]
    mov byte [d], 1
    mov byte [d+5], 1
    mov word [d+2], 1
    mov dword [d+rax], 1
    add qword [d], 1
    lock add qword [d+8], 1
    lock xadd qword [d+8], rax
    movzx eax, byte [d+rbx]
    movsxd rax, dword [d+8]
    mov rax, d
    mov rax, d+40
    mov rax, bs
    mov rax, bs+7
    mov rax, ro
    mov r15, ro+3
    mov [bs], rax
    mov [bs+8], rbx
    mov [rel bs+8], rbx
    mov rax, _start
    mov rax, _start+2
    ret
section .data
d:  times 8 dq 0
section .rodata
ro: db 1
section .bss
bs: resq 4
;---
; M3: data symbols also from a global label, jmp table in the middle of .text
global tbl
global ent
section .text
ent:
    call ent
    mov rax, tbl
    jmp [tbl]
    jmp qword [tbl+8]
    call [tbl+16]
    dd ent, tail
    dq ent, tail, tbl
tail:
    ret
section .data
tbl: dq ent, tail, ent+1
;---
; M3: long db (256 values) and a big .bss
section .data
big: db 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
section .bss
huge: resb 4194304
more: resq 1048576
;---
; M3: comments, blank lines, labels with colons alone, tabs
section	.data
; a comment
x:
	db	1 , 2 ,	3	; tail
y:	dq	x
	; indented comment

section .bss
z:	resb	8	; eight
