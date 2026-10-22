// Freestanding: port I/O builtins and inline assembly (no OS, no syscalls).
fn serial_putc(c: i64) -> i64 {
    while ((inb(0x3FD) & 0x20) == 0) { pause(); }
    outb(0x3F8, c);
    return 0;
}

fn kmain() -> i64 {
    cli();
    serial_putc('O');
    serial_putc('K');
    let w = inw(0x60);
    outw(0x70, w);
    outl(0xCF8, inl(0xCFC));
    asm("mov rax, cr0; nop");
    hlt();
    return 0;
}
