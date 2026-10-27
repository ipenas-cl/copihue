; Hand-written edge cases for compiler/native/x64.c. One instruction per line.
; Symbols (foo, bar) are extern in the nasm object.
mov rax, rbx
mov r8, r15
mov r15, rax
mov eax, ebx
mov r9d, r10d
mov ax, bx
mov r11w, ax
mov al, bl
mov sil, dil
mov spl, bpl
mov r8b, r15b
mov al, r9b
mov ah, bl
mov ch, dh
mov rax, [rbx]
mov rax, [rsp]
mov rax, [rbp]
mov rax, [r12]
mov rax, [r13]
mov rax, [r12+8]
mov rax, [r13+8]
mov rax, [rbp-8]
mov rax, [rbp-129]
mov rax, [rbp+127]
mov rax, [rbp+128]
mov rax, [rsp+1000]
mov rax, [rax+rbx]
mov rax, [rax+rbx*2]
mov rax, [rax+rbx*4+16]
mov rax, [r8+r9*8-1000]
mov rax, [rbp+rcx*8]
mov rax, [r13+rcx*8]
mov rax, [rsp+rcx*8]
mov rax, [r12+r15*8]
mov rax, [rax+r12]
mov rax, [rbx*4+100]
mov rax, [rbx*8]
mov rax, [foo]
mov rax, [foo+8]
mov rax, [foo+rbx*8]
mov rax, [foo+rbx*8-4]
mov rax, [rbx+foo]
mov [foo], rax
mov [foo], r12
mov [foo], eax
mov [foo], al
mov [foo+16], sil
mov [rbx], rax
mov [rbx], eax
mov [rbx], ax
mov [rbx], al
mov [rbx], spl
mov [r9], r10b
mov eax, [rbx]
mov ax, [rbx]
mov al, [rbx]
mov sil, [rbx]
mov r9b, [rbx+r10]
mov rax, 0
mov rax, 1
mov rax, 0x7fffffff
mov rax, 0x80000000
mov rax, 0xffffffff
mov rax, 0x100000000
mov rax, -1
mov rax, -2147483648
mov rax, -2147483649
mov r9, 5
mov r9, -5
mov r9, 0x123456789
mov rax, foo
mov rax, foo+8
mov r13, foo
mov eax, 5
mov eax, -1
mov eax, 0xffffffff
mov r9d, 7
mov eax, foo
mov ax, 5
mov ax, 0xffff
mov r9w, 300
mov al, 5
mov al, -1
mov al, 255
mov ah, 3
mov sil, 3
mov r9b, 3
mov byte [rbx], 5
mov byte [rbp-1], 45
mov byte [foo], 5
mov word [rbx], 5
mov word [rbx], 1000
mov dword [rbx], 5
mov dword [rbx], -1
mov dword [rbx+rcx*4], 0x12345678
mov dword [foo], 7
mov dword [foo], bar
mov qword [rbx], 5
mov qword [rbx], -5
mov qword [rbx], 0x7fffffff
mov qword [rbx], foo
mov qword [rbp-8], 0
mov qword [r12+8], 100
add rax, rbx
add [rbx], rax
add rax, [rbx]
add eax, 1
add rax, 1
add rax, -1
add rax, 127
add rax, 128
add rax, -128
add rax, -129
add rax, 1000
add rax, 0x7fffffff
add rbx, 1000
add r9, 1000
add rax, foo
add rbx, foo
add eax, 1000
add eax, 0xffffffff
add ax, 1000
add ax, 1
add al, 5
add al, -3
add bl, 5
add sil, 5
add byte [rbx], 5
add word [rbx], 1000
add dword [rbx], 1000
add qword [rbx], 1
add qword [rbx], 1000
add qword [rbp-8], 1
add qword [foo], 1
lock add qword [rax], 1
lock add dword [rax], ebx
or rax, rbx
or rax, 1
or qword [rbx], 8
adc rax, rbx
sbb rax, 1
and rax, rbx
and rax, 0xff
and rax, -16
and eax, 0xff
and r9, 7
sub rax, rbx
sub rsp, 8
sub rsp, 4096
sub rsp, 32
xor rax, rax
xor eax, eax
xor r9d, r9d
xor rdx, rdx
xor al, al
xor rax, [rbx]
cmp rax, rbx
cmp rax, 0
cmp rax, 1000
cmp rax, -1
cmp eax, 5
cmp al, 5
cmp al, bl
cmp sil, 5
cmp rdi, rax
cmp rdi, 2
cmp byte [rdi+rax], 0
cmp byte [rbx], 65
cmp dword [rbx], 5
cmp qword [rbx], 5
cmp qword [rbx], 1000
cmp qword [foo], 0
cmp [rbx], rax
cmp rax, [rbx+8]
test rax, rax
test eax, eax
test al, al
test bl, bl
test sil, sil
test r9, r9
test rax, 1
test rax, 0xff
test rbx, 0xff
test al, 1
test bl, 1
test eax, 1
test [rbx], rax
test rax, [rbx]
test byte [rbx], 1
test qword [rbx], 1
lea rax, [rbx]
lea rax, [rbx+8]
lea rax, [rbx+rcx*4]
lea rax, [rbp-32]
lea rsi, [rbp-1]
lea rdi, [rbp-1000]
lea r9, [r12+r13*8+5]
lea eax, [rbx+rcx]
lea rax, [foo]
lea rax, [foo+rbx*8]
lea rax, [rax+rax*2]
inc rax
inc eax
inc al
inc r9
inc dword [rbx]
inc qword [rbx]
inc byte [rbx]
dec rax
dec r12
dec qword [rbp-8]
neg rax
neg r9d
neg qword [rbx]
not rax
not al
mul rbx
mul qword [rbx]
div rcx
div r9
div ecx
idiv rcx
idiv qword [rbx]
idiv r10
imul rbx
imul rax, rbx
imul rax, [rbx]
imul r9, r10
imul eax, ebx
imul rax, rbx, 10
imul rax, rbx, 1000
imul rax, rbx, -3
imul rax, 10
imul rax, 1000
imul rax, [rbx], 5
imul r9, 7
shl rax, 1
shl rax, 3
shl rax, cl
shl eax, 2
shl r9, 4
shl al, 1
shl bl, 3
shl byte [rbx], 1
shl qword [rbx], 5
shl qword [rbx], cl
shr rax, 1
shr rax, 63
shr rax, cl
shr eax, 8
sar rax, 1
sar rax, 3
sar rax, cl
sar r9, 2
rol rax, 8
ror rax, 8
push rax
push rbx
push r8
push r15
push 0
push 5
push 127
push 128
push -1
push -128
push -129
push 100000
push foo
push qword [rbx]
push qword [rbp-8]
pop rax
pop r9
pop r15
pop qword [rbx]
call foo
call rax
call r9
call [rax]
call [foo]
call qword [rbx+8]
jmp foo
jmp rax
jmp r12
jmp [rax]
jmp qword [foo+rax*8]
je foo
jne foo
jz foo
jnz foo
jl foo
jge foo
jle foo
jg foo
js foo
jns foo
jae foo
jb foo
ja foo
jbe foo
jc foo
jnc foo
sete al
setne bl
setz r9b
setc sil
setl al
setg al
setge al
setle al
seta al
setb al
sete byte [rbx]
setne byte [foo]
cmove rax, rbx
cmovl rax, [rbx]
cmovg r9, r10
cmovne eax, ebx
movzx rax, al
movzx eax, al
movzx eax, bl
movzx rax, sil
movzx r9, r10b
movzx eax, ax
movzx rax, bx
movzx rax, byte [rbx]
movzx eax, byte [rbx+rcx]
movzx rax, word [rbx]
movzx eax, word [foo]
movsx rax, al
movsx eax, bl
movsx rax, cx
movsx rax, byte [rbx]
movsx rax, word [rbx]
movsx r9, sil
movsxd rax, eax
movsxd rax, ebx
movsxd r9, r10d
movsxd rax, dword [rbx]
movsxd rax, dword [rbp-8]
xchg rax, rbx
xchg rbx, rax
xchg rbx, rcx
xchg [rbx], rax
xchg rax, [rbx]
xchg eax, ebx
xchg al, bl
xchg r9, rax
xadd [rax], rbx
xadd [rax], ebx
xadd [rax], bl
lock xadd [rax], rbx
lock xadd qword [rax], r12
cmpxchg [rax], rbx
lock cmpxchg [rax], rbx
lock cmpxchg [rax], ecx
lock cmpxchg [rax], dl
ret
leave
syscall
cqo
cdq
cld
std
hlt
ud2
nop
int3
pushfq
popfq
stc
clc
cpuid
rdtsc
pause
mfence
lfence
sfence
rep movsb
rep movsq
rep stosb
rep stosq
rep stosd
movsb
movsq
paddb xmm0, xmm1
paddb xmm9, xmm2
paddq xmm1, [rbx]
pxor xmm0, xmm0
pxor xmm8, xmm15
punpcklbw xmm0, xmm1
punpcklwd xmm2, xmm3
punpckldq xmm2, xmm3
psadbw xmm0, xmm1
pshufd xmm0, xmm1, 0xe0
pshufd xmm2, [rbx], 0
psrldq xmm0, 8
movdqu xmm0, [rbx]
movdqu [rbx], xmm1
movdqu xmm1, xmm2
movdqu xmm9, [r12+8]
movdqa xmm0, [rbx]
movdqa [rbp-16], xmm3
movd xmm0, eax
movd eax, xmm0
movd xmm9, r10d
movq xmm0, rax
movq rax, xmm0
movq xmm9, r10
movdqu xmm2, [rel foo]
movdqu xmm2, [rel foo+16]
pshufd xmm2, [rel foo], 3
