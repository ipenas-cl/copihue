; Invalid instructions: compiler/native/x64.c must refuse each one, and nasm
; must refuse it too (tests/x64/run.sh, mode `reject`). A line that nasm accepts
; does not belong here: it is a feature x64.c may add, not an error.
frobnicate rax
mov rax, [rsp*2]
mov rax, [rsp+rsp]
mov rax, [rax+rbx*3]
mov rax, [rax+rbx+rcx]
mov rax, [rax+rbx*2+rcx*2]
mov eax, rbx
mov al, ax
mov ax, rbx
mov rax, rbx, rcx
mov rax
mov [rax], [rbx]
mov 1, rax
mov ah, r8b
mov r8b, ah
mov ah, [r8]
mov sil, ah
add [rax], [rbx]
add rax, eax
add 5, rax
lea rax, rbx
push al
push eax
pop eax
xchg rax, 5
xchg [rax], [rbx]
movzx rax, rbx
movzx rax, 5
movsx al, bl
shl rax, rbx
shl rax, dl
inc 5
ret rax
syscall rax
call rax, rbx
jmp
cmp rax
test rax
mov xmm0, rax
movdqu xmm0, rax
paddb xmm0, rax
pshufd xmm0, xmm1
movd xmm0, rax
; encoder-only: rejected by x64.c; nasm 3.x accepts these (2.x refuses some), so nasm is not asked
mov [rax], 1
lea rax, 1
movsxd eax, eax
imul rax, rbx, rcx
inc [rax]
