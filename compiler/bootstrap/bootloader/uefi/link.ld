/* Linker script for UEFI bootloader */

OUTPUT_FORMAT("pe-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0;
    ImageBase = .;
    
    /* PE/COFF headers */
    .text : {
        *(.text.efi_main)  /* Entry point first */
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        . = ALIGN(16);
    }
    
    .data : {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(16);
    }
    
    .reloc : {
        *(.reloc)
        . = ALIGN(16);
    }
    
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.debug*)
    }
}