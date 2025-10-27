#!/bin/bash
# ╔═════╦═════╦═════╗
# ║ 🛡️  ║ ⚖️  ║ ⚡  ║
# ║  C  ║  E  ║  G  ║
# ╚═════╩═════╩═════╝
# ╔═════════════════╗
# ║ wcet [T∞] bound ║
# ╚═════════════════╝
#
# Author: Ignacio Peña Sepúlveda
# Date: June 25, 2025
#
# AtomicOS Bootloader Build Script

set -e

echo "Building AtomicOS bootloader..."

# Create output directories
mkdir -p bios
mkdir -p uefi
mkdir -p build

# Build BIOS Stage 1
echo "Building BIOS Stage 1..."
nasm -f bin -o bios/boot.bin bios/boot.s

# Create assembly helpers for Stage 2
echo "Creating assembly helpers..."
cat > bios/stage2_asm.s << 'EOF'
; Assembly support functions for BIOS stage2
[BITS 32]
section .text

global outb
global inb
global lgdt
global lidt
global enable_paging
global jump_to_kernel

outb:
    mov dx, [esp+4]
    mov al, [esp+8]
    out dx, al
    ret

inb:
    mov dx, [esp+4]
    in al, dx
    ret

lgdt:
    mov eax, [esp+4]
    lgdt [eax]
    ret

lidt:
    mov eax, [esp+4]
    lidt [eax]
    ret

enable_paging:
    mov eax, [esp+4]    ; PML4 address
    mov cr3, eax
    ret

jump_to_kernel:
    ; Jump to 64-bit kernel
    ; Parameters: entry point, multiboot info
    cli
    
    ; Enable long mode
    mov eax, cr4
    or eax, 0x20        ; Set PAE bit
    mov cr4, eax
    
    mov ecx, 0xC0000080 ; EFER MSR
    rdmsr
    or eax, 0x100       ; Set LM bit
    wrmsr
    
    mov eax, cr0
    or eax, 0x80000001  ; Enable paging and protection
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64.pointer]
    
    ; Jump to 64-bit code
    jmp 0x08:long_mode_start
    
section .rodata
gdt64:
    dq 0    ; null descriptor
.code:
    dq 0x00209A0000000000  ; 64-bit code descriptor
.data:
    dq 0x0000920000000000  ; data descriptor
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

[BITS 64]
section .text
long_mode_start:
    ; Set up segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Get parameters from stack
    mov rdi, [rsp+8]    ; Entry point
    mov rsi, [rsp+16]   ; Multiboot info
    
    ; Set multiboot magic
    mov eax, 0x2BADB002
    mov ebx, esi        ; Multiboot info in EBX
    
    ; Jump to kernel
    jmp rdi
EOF

# Build Stage 2 assembly
echo "Assembling Stage 2 helpers..."
nasm -f elf32 -o bios/stage2_asm.o bios/stage2_asm.s

# Note: The actual Tempo compilation would happen here
# For now, create a stub Stage 2
echo "Creating Stage 2 stub..."
dd if=/dev/zero of=bios/stage2.bin bs=512 count=64

# Create disk image
echo "Creating disk image..."
dd if=/dev/zero of=boot.img bs=1M count=10
dd if=bios/boot.bin of=boot.img conv=notrunc
dd if=bios/stage2.bin of=boot.img bs=512 seek=1 conv=notrunc

# Create UEFI stub
echo "Creating UEFI bootloader stub..."
mkdir -p uefi/EFI/BOOT
echo -ne '\x4D\x5A' > uefi/EFI/BOOT/BOOTX64.EFI  # PE header stub

echo "Build complete!"
echo ""
echo "To test with QEMU:"
echo "  BIOS: qemu-system-x86_64 -drive file=boot.img,format=raw"
echo "  UEFI: qemu-system-x86_64 -drive file=boot.img,format=raw -bios /usr/share/ovmf/OVMF.fd"