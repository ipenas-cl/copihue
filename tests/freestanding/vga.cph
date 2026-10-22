// Freestanding: write to VGA text memory with 16-bit stores.
let row: i64 = 0;

fn vga_put(col: i64, ch: i64) -> i64 {
    let vga: *u16 = 0xB8000;
    vga[row * 80 + col] = 0x0F00 | ch;
    return 0;
}

fn kmain() -> i64 {
    vga_put(0, 'H');
    vga_put(1, 'i');
    return 0;
}
