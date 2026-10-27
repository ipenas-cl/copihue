; Casos de borde del codificador x86-64: todos los registros, bases que exigen
; SIB o disp8 (rsp, r12, rbp, r13), límites de inmediatos y de desplazamientos.
mov rax, rbx
mov r8, r15
mov r15, r8
mov eax, r9d
mov r10d, ecx
mov ax, r11w
mov r12w, bx
mov al, bl
mov sil, dil
mov spl, bpl
mov r8b, r15b
mov al, r9b
mov ah, bl
mov ch, dh
mov rax, [rax]
mov rax, [rsp]
mov rax, [rbp]
mov rax, [r12]
mov rax, [r13]
mov rax, [rsp+8]
mov rax, [rbp+8]
mov rax, [r12+8]
mov rax, [r13+8]
mov rax, [rax+127]
mov rax, [rax+128]
mov rax, [rax-128]
mov rax, [rax-129]
mov rax, [rax+0x7FFFFFFF]
mov rax, [rax-0x80000000]
mov rax, [rax+rbx]
mov rax, [rax+rbx*2]
mov rax, [rax+rbx*4]
mov rax, [rax+rbx*8]
mov rax, [rbp+rbx*8]
mov rax, [r13+r12*8]
mov rax, [rsp+r11]
mov rax, [r12+r13*4+16]
mov rax, [r8+r9*2-300]
mov [rax+rcx*8+24], r15
mov [r12], rax
mov [r13], rax
mov byte [r12], sil
mov byte [r13+1], dil
mov dword [rsp+4], r9d
mov word [rbp-2], r10w
mov qword [rax], 0
mov qword [rax], -1
mov qword [rax], 0x7FFFFFFF
mov qword [rax], -0x80000000
mov dword [rax], 0xFFFFFFFF
mov dword [rax], 0x80000000
mov word [rax], 0xFFFF
mov word [rax], -1
mov byte [rax], 255
mov byte [rax], -128
mov rax, 0
mov rax, 1
mov rax, 0xFFFFFFFF
mov rax, 0x100000000
mov rax, -1
mov rax, -0x80000000
mov rax, -0x80000001
mov rax, 0x7FFFFFFFFFFFFFFF
mov r9, 0xFFFFFFFF
mov r9, -1
mov r9, 0x123456789A
mov eax, 0x12345678
mov r9d, 0x12345678
mov ax, 0x1234
mov r10w, 0x1234
mov al, 7
mov r9b, 7
mov sil, 7
mov dil, 255
add rax, 1
add rax, 127
add rax, 128
add rax, -128
add rax, -129
add rax, 0x7FFFFFFF
add rbx, 1
add rbx, 128
add r9, 1
add r9, 1000
add eax, 1
add eax, 128
add eax, 0xFFFFFFFF
add ebx, 0xFFFFFFFF
add ax, 1
add ax, 1000
add bx, 1000
add al, 1
add al, 200
add bl, 1
add sil, 1
add dword [rax], 1
add dword [rax], 1000
add qword [rbp-8], 5
add byte [rax], 5
add word [rax], 5
add word [rax], 5000
sub rsp, 8
sub rsp, 200
or rax, rbx
or rax, 1
and rax, -16
and rsp, -16
and eax, 0xFFFFFFF0
xor eax, eax
xor r9d, r9d
xor rax, rax
xor al, al
cmp rax, 0
cmp rax, 1000
cmp rbx, 1000
cmp eax, 1000
cmp ax, 1000
cmp al, 1
cmp bl, 1
cmp byte [rax], 0
cmp byte [rax+rbx], 0
cmp dword [rax], 1000
cmp qword [rax], -1
cmp rax, [rbx]
cmp [rbx], rax
cmp [rbx], r8d
cmp al, [rbx]
adc rax, rbx
sbb rax, 1
test rax, rax
test eax, eax
test al, al
test sil, sil
test r9, r10
test [rax], rbx
test [rax], bl
push rax
push r8
push r15
push 0
push 127
push 128
push -128
push -129
push 0x7FFFFFFF
pop rax
pop r8
pop r15
lea rax, [rbx]
lea rax, [rbx+rcx*8+16]
lea r9, [r10+r11*4]
lea rax, [rsp+8]
lea rax, [rbp-8]
lea eax, [rbx+rbx*2]
lea r13, [r12+r12*2]
inc rax
inc r8
inc eax
inc al
inc sil
dec rax
dec qword [rax]
dec dword [rbp-4]
inc byte [rax]
inc word [rax]
neg rax
neg r9
neg eax
not rax
not r9
mul rbx
div rcx
div r8
idiv rbx
idiv r9
idiv qword [rax]
div dword [rax]
imul rax, rbx
imul r8, r9
imul rax, [rbx]
imul eax, ebx
imul rax, rbx, 3
imul rax, rbx, 1000
imul rax, rax, 1103515245
imul rax, 4
imul r9, 4
imul r9, r10, 100
imul ax, bx
shl rax, 1
shl rax, 2
shl rax, 63
shl r9, 1
shl eax, 1
shl eax, cl
shl rax, cl
shl al, 1
shl al, 3
shl al, cl
shl qword [rax], 1
shl dword [rax], 5
shr rax, 1
shr r11, 3
sar rdx, 63
sar rax, cl
rol rax, 8
ror eax, 3
movzx eax, al
movzx rax, al
movzx rax, bl
movzx rax, sil
movzx r9, r10b
movzx rax, ax
movzx eax, bx
movzx rax, byte [rbx]
movzx rax, word [rbx]
movzx eax, byte [rbx+rcx*2+4]
movzx r9, byte [r10]
movzx ax, bl
movsx rax, al
movsx rax, ax
movsx eax, bl
movsx rax, byte [rbx]
movsx rax, word [rbx]
movsxd rax, eax
movsxd r9, r10d
movsxd rax, dword [rbx]
movsxd rax, dword [rbx+rcx*4-8]
sete al
setz al
setne bl
setg sil
setl r9b
setge al
setle al
seta al
setb al
setc al
setae al
setbe al
sete byte [rax]
call rax
call r12
jmp rax
jmp r9
xchg qword [r11], rax
xchg rax, qword [r11]
xchg [rax], rbx
xchg [rax], bl
xchg rax, rbx
lock xadd qword [r11], rax
lock xadd dword [r11], eax
lock xadd byte [r11], al
lock cmpxchg qword [r11], r10
lock cmpxchg dword [rax], ecx
lock add qword [rax], 1
lock add dword [rax], 1
lock sub qword [rax], 128
lock inc qword [rax]
lock dec qword [rax]
lock or qword [rax], rbx
lock and qword [rax], rbx
syscall
ret
leave
hlt
cld
std
ud2
pause
mfence
lfence
sfence
pushfq
popfq
stc
clc
cpuid
cqo
cdq
cdqe
nop
int3
rdtsc
rep movsb
rep stosq
rep stosb
rep movsq
movsb
stosq
movdqu xmm0, [rax]
movdqu xmm9, [r8+r9*8+32]
movdqu [rax], xmm0
movdqu [r13], xmm15
movdqu xmm1, xmm2
movdqa xmm4, xmm0
movdqa [rsp], xmm8
movdqa xmm8, [rsp+16]
paddb xmm0, xmm2
paddq xmm3, xmm4
paddq xmm9, [rax]
paddd xmm1, xmm1
pxor xmm2, xmm2
pxor xmm10, xmm11
por xmm1, xmm2
pand xmm1, xmm2
pcmpeqb xmm0, xmm1
punpcklbw xmm1, xmm1
punpcklwd xmm1, xmm1
punpckldq xmm1, xmm1
psadbw xmm4, xmm2
pshufd xmm1, xmm1, 0
pshufd xmm9, xmm8, 0xE4
pshufd xmm1, [rax], 27
psrldq xmm3, 8
psrldq xmm12, 4
movd xmm1, eax
movd xmm9, r10d
movd eax, xmm1
movd [rax], xmm1
movd xmm1, [rax]
movq rax, xmm3
movq xmm3, rax
movq r9, xmm10
movq xmm10, r9
F_V16 equ 16
F_V64 equ 64
F_V200 equ 200
F_V4000 equ 4000
sub rsp, F_V16
sub rsp, F_V64
sub rsp, F_V200
sub rsp, F_V4000
mov rax, [rbp - F_V16 + 8]
mov rax, [rbp - F_V64 + 8]
mov rax, [rbp - F_V200 + 8]
mov [rbp - F_V4000 + 8], r12
mov qword [rbp - F_V64 + 16], 0
add rsp, F_V200
lea rax, [rbp - F_V200]
mov rax, F_V200
mov eax, F_V16
cmp rax, F_V4000
push F_V16
push F_V200
