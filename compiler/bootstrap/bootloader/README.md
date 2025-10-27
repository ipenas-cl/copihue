<div align="center">

╔═════╦═════╦═════╗  
║ 🛡️  ║ ⚖️  ║ ⚡  ║  
║  C  ║  E  ║  G  ║  
╚═════╩═════╩═════╝  
╔═════════════════╗  
║ wcet [T∞] bound ║  
╚═════════════════╝  

**Author:** Ignacio Peña Sepúlveda  
**Date:** June 25, 2025

</div>

---



## Features

- **Dual Boot Support**: Works with both BIOS and UEFI firmware
- **Two-Stage BIOS Boot**: Minimal first stage + full-featured second stage
- **UEFI GOP Support**: Graphics Output Protocol for modern graphics
- **Memory Detection**: Full memory map detection for both BIOS and UEFI
- **ELF Kernel Loading**: Loads 64-bit ELF kernels
- **Multiboot Compliance**: Passes Multiboot 1 information to kernel
- **Long Mode Setup**: Transitions from 16-bit to 64-bit mode
- **VGA Text Output**: Basic console output during boot

## Architecture

### BIOS Boot Path

1. **Stage 1 (boot.s)**: 
   - Loaded by BIOS at 0x7C00
   - Enables A20 line
   - Loads Stage 2 from disk
   - Jumps to Stage 2

2. **Stage 2 (stage2.ch)**:
   - Runs in real mode initially
   - Detects system memory
   - Sets up GDT and page tables
   - Loads kernel ELF file
   - Enables long mode
   - Jumps to kernel with Multiboot info

### UEFI Boot Path

1. **UEFI Application (boot.ch)**:
   - Loaded by UEFI firmware
   - Uses UEFI protocols for:
     - Memory detection
     - File system access
     - Graphics initialization
   - Loads kernel from EFI system partition
   - Exits boot services
   - Jumps to kernel with Multiboot info

## Memory Layout

### BIOS Mode
- `0x0000-0x7BFF`: Real mode memory
- `0x7C00-0x7DFF`: Stage 1 bootloader
- `0x8000-0xFFFF`: Stage 2 bootloader
- `0x100000-0x103FFF`: Page tables
- `0x200000+`: Kernel

### UEFI Mode
- Memory allocated dynamically by UEFI
- Kernel loaded at preferred address from ELF headers

## Building

```bash
# Build everything
make all

# Build only BIOS bootloader
make bios

# Build only UEFI bootloader
make uefi

# Create bootable disk image
make disk

# Clean build artifacts
make clean
```

## Testing

```bash
# Test BIOS boot
make test-bios

# Test UEFI boot
make test-uefi
```

## File Structure

```
bootloader/
├── bios/
│   ├── boot.s          # Stage 1 bootloader (assembly)
│   ├── stage2.ch    # Stage 2 bootloader (Chronos)
│   └── link.ld         # Linker script for BIOS
├── uefi/
│   ├── boot.ch      # UEFI bootloader (Chronos)
│   ├── uefi.ch      # UEFI protocol definitions
│   └── link.ld         # Linker script for UEFI
├── common/
│   ├── multiboot.ch # Multiboot structures
│   ├── memory.ch    # Memory management
│   ├── vga.ch       # VGA text output
│   └── elf.ch       # ELF loader
├── Makefile            # Build system
├── build.sh            # Alternative build script
└── README.md           # This file
```

## Multiboot Information

The bootloader passes the following information to the kernel:

- Memory map (BIOS E820 or UEFI memory map)
- Total available memory
- Framebuffer information (resolution, address, format)
- Boot device information
- Command line (if provided)

## Requirements

- NASM assembler
- Chronos compiler
- GNU LD linker
- QEMU (for testing)
- OVMF UEFI firmware (for UEFI testing)

## Known Limitations

- BIOS stage 2 limited to 32KB
- Only supports loading kernels from first partition
- Requires kernel to be in ELF format
- UEFI loader expects kernel at `/EFI/BOOT/kernel.elf`

## Future Improvements

- Support for compressed kernels
- Better error handling and recovery
- Support for loading modules
- Graphical boot menu
- Secure Boot support
- Support for other architectures