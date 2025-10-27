# AtomicOS Bootloader Makefile

# Tools
AS = nasm
TC = tempoc  # Tempo compiler
LD = ld
OBJCOPY = objcopy
DD = dd

# Flags
ASFLAGS = -f elf32
TCFLAGS = -target i386 -fno-stack-protector -ffreestanding -nostdlib
LDFLAGS = -m elf_i386 -nostdlib

# UEFI flags
UEFI_TCFLAGS = -target x86_64 -fno-stack-protector -ffreestanding -nostdlib -mno-red-zone
UEFI_LDFLAGS = -m elf_x86_64 -nostdlib -Wl,-dll -shared -Wl,--subsystem,10

# Output files
BIOS_STAGE1 = bios/boot.bin
BIOS_STAGE2 = bios/stage2.bin
UEFI_LOADER = uefi/BOOTX64.EFI
DISK_IMAGE = boot.img

# Common modules
COMMON_SRCS = common/multiboot.tempo common/memory.tempo common/vga.tempo common/elf.tempo

.PHONY: all clean bios uefi disk

all: bios uefi disk

# BIOS bootloader
bios: $(BIOS_STAGE1) $(BIOS_STAGE2)

$(BIOS_STAGE1): bios/boot.s
	$(AS) -f bin -o $@ $<

bios/stage2.o: bios/stage2.tempo $(COMMON_SRCS)
	$(TC) $(TCFLAGS) -c -o $@ $<

bios/stage2_asm.o: bios/stage2_asm.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BIOS_STAGE2): bios/stage2.o bios/stage2_asm.o
	$(LD) $(LDFLAGS) -T bios/link.ld -o bios/stage2.elf $^
	$(OBJCOPY) -O binary bios/stage2.elf $@

# Assembly support functions for stage2
bios/stage2_asm.s: 
	@echo '; Assembly support functions for BIOS stage2' > $@
	@echo 'section .text' >> $@
	@echo 'global outb' >> $@
	@echo 'global inb' >> $@
	@echo 'global lgdt' >> $@
	@echo 'global lidt' >> $@
	@echo 'global enable_paging' >> $@
	@echo 'global jump_to_kernel' >> $@
	@echo '' >> $@
	@echo 'outb:' >> $@
	@echo '    mov dx, [esp+4]' >> $@
	@echo '    mov al, [esp+8]' >> $@
	@echo '    out dx, al' >> $@
	@echo '    ret' >> $@
	@echo '' >> $@
	@echo 'inb:' >> $@
	@echo '    mov dx, [esp+4]' >> $@
	@echo '    in al, dx' >> $@
	@echo '    ret' >> $@
	@echo '' >> $@
	@echo 'lgdt:' >> $@
	@echo '    mov eax, [esp+4]' >> $@
	@echo '    lgdt [eax]' >> $@
	@echo '    ret' >> $@
	@echo '' >> $@
	@echo 'lidt:' >> $@
	@echo '    mov eax, [esp+4]' >> $@
	@echo '    lidt [eax]' >> $@
	@echo '    ret' >> $@
	@echo '' >> $@
	@echo 'jump_to_kernel:' >> $@
	@echo '    ; Jump to 64-bit kernel' >> $@
	@echo '    ; Parameters: rdi = entry point, rsi = multiboot info' >> $@
	@echo '    mov eax, 0x2BADB002  ; Multiboot magic' >> $@
	@echo '    mov ebx, [esp+8]     ; Multiboot info pointer' >> $@
	@echo '    mov ecx, [esp+4]     ; Kernel entry point' >> $@
	@echo '    ; Switch to long mode and jump' >> $@
	@echo '    jmp 0x08:long_mode_entry' >> $@
	@echo '' >> $@
	@echo '[BITS 64]' >> $@
	@echo 'long_mode_entry:' >> $@
	@echo '    mov rax, 0x10' >> $@
	@echo '    mov ds, ax' >> $@
	@echo '    mov es, ax' >> $@
	@echo '    mov fs, ax' >> $@
	@echo '    mov gs, ax' >> $@
	@echo '    mov ss, ax' >> $@
	@echo '    mov rdi, rcx  ; Entry point' >> $@
	@echo '    mov rsi, rbx  ; Multiboot info' >> $@
	@echo '    jmp rdi' >> $@

# UEFI bootloader
uefi: $(UEFI_LOADER)

uefi/boot.o: uefi/boot.tempo uefi/uefi.tempo $(COMMON_SRCS)
	$(TC) $(UEFI_TCFLAGS) -c -o $@ $<

$(UEFI_LOADER): uefi/boot.o
	$(LD) $(UEFI_LDFLAGS) -T uefi/link.ld -o uefi/BOOTX64.so $<
	$(OBJCOPY) -j .text -j .data -j .reloc -O binary uefi/BOOTX64.so $@

# Create bootable disk image
disk: $(DISK_IMAGE)

$(DISK_IMAGE): $(BIOS_STAGE1) $(BIOS_STAGE2)
	# Create 100MB disk image
	dd if=/dev/zero of=$@ bs=1M count=100
	# Write MBR boot sector
	dd if=$(BIOS_STAGE1) of=$@ conv=notrunc
	# Write stage2 at sector 1
	dd if=$(BIOS_STAGE2) of=$@ bs=512 seek=1 conv=notrunc
	# Create partition table
	@echo "Creating partition table..."
	@echo -e "o\nn\np\n1\n2048\n\nt\nef\nw" | fdisk $@ || true
	# Format partition as FAT32 and copy UEFI loader
	@sudo losetup -P /dev/loop0 $@
	@sudo mkfs.fat -F 32 /dev/loop0p1
	@sudo mkdir -p /mnt/atomicos
	@sudo mount /dev/loop0p1 /mnt/atomicos
	@sudo mkdir -p /mnt/atomicos/EFI/BOOT
	@sudo cp $(UEFI_LOADER) /mnt/atomicos/EFI/BOOT/
	@sudo umount /mnt/atomicos
	@sudo losetup -d /dev/loop0

# Test with QEMU
test-bios: $(DISK_IMAGE)
	qemu-system-x86_64 -drive file=$(DISK_IMAGE),format=raw

test-uefi: $(DISK_IMAGE)
	qemu-system-x86_64 -drive file=$(DISK_IMAGE),format=raw -bios /usr/share/ovmf/OVMF.fd

clean:
	rm -f bios/*.o bios/*.bin bios/*.elf
	rm -f uefi/*.o uefi/*.so uefi/*.EFI
	rm -f $(DISK_IMAGE)