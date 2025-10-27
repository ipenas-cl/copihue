/* Linker script for BIOS Stage 2 bootloader */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(stage2_main)

SECTIONS
{
    . = 0x8000;  /* Stage 2 is loaded at 0x8000 */
    
    .text : {
        *(.text.entry)  /* Entry point first */
        *(.text)
        *(.text.*)
    }
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
    }
    
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /* Ensure we don't exceed 32KB (64 sectors) */
    . = ASSERT(. <= 0x10000, "Stage 2 bootloader exceeds 32KB!");
    
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
    }
}