# Kernels: freestanding, flat and boot modes

Moved out of the README on 2026-10-26. Unchanged except for heading levels.

## Freestanding mode (kernels)

```copihue
fn serial_putc(c: i64) -> i64 {
    while ((inb(0x3FD) & 0x20) == 0) { pause(); }
    outb(0x3F8, c);
    return 0;
}
```

`copihue --freestanding -o kernel.asm kernel.lap` produces NASM with no `_start`, no syscalls and no runtime.
Your own entry code `%include`s it and calls your functions using the System V argument registers.
Hosted built-ins such as `println` are a compile error in this mode.

The same mode can express the pieces a small kernel needs after it is already in long mode:

```copihue
packed struct IdtEntry {
    off_lo: u16, sel: u16, ist: u8, typ: u8,
    off_mid: u16, off_hi: u32, zero: u32
}

let volatile ticks: i64 = 0;
let idt: [IdtEntry; 32];

interrupt fn timer() {
    ticks = ticks + 1;
    outb(0x20, 0x20);
}

fn set_gate(n: i64, handler: i64) -> i64 {
    idt[n].off_lo = handler & 0xFFFF;
    idt[n].sel = 0x18;
    idt[n].typ = 0x8E;
    idt[n].off_mid = (handler >> 16) & 0xFFFF;
    idt[n].off_hi = handler >> 32;
    return 0;
}
```

`set_gate(32, &timer)` then `lidt(&idtr)` installs it. The selector `0x18` is the 64-bit code segment of the AtomicOS bootloader GDT; `0x08` on that loader is 32-bit code and a gate that uses it hangs. `tests/flat/min_kernel.lap` is a complete `--flat 0x10000` image for that GDT, not a generic bootable kernel: it clears the BSS, writes a byte to the serial port, bump-allocates from a global buffer, builds a packed IDT, and defines an interrupt handler and a syscall entry. A `syscall fn` takes the Linux register convention (`rax` = number, `rdi`/`rsi`/`rdx`/`r10`/`r8`/`r9` = arguments), switches to the stack given to `set_syscall_stack`, and returns with `o64 sysret` (sysretq). `tests/flat/user_syscall.lap` is the `ltr` plus `EFER`/`STAR`/`LSTAR`/`SFMASK` sequence. `tests/usermode` is the same sequence booted in QEMU through to ring 3; its GDT is its own, not the AtomicOS loader GDT.

### Flat binaries (`--flat`)

`copihue --flat 0x10000 -o kernel.bin kernel.lap` goes one step further: it builds a raw binary (via `nasm -f bin`) meant to be loaded at the given address, with **no assembly entry stub**. Execution starts at the first byte, which is always `fn _start` (it is emitted first wherever it is defined; a missing `_start` is an error). Two extra built-ins, `bss_start()` and `bss_end()`, return the bounds of the zero-initialized globals so `_start` can clear them. The [AtomicOS](https://github.com/ipenas-cl/AtomicOS) kernel is built this way. Add `-S` to see the assembly instead.

### Boot sector (`--boot`)

`copihue --boot -o boot.bin boot.lap` builds that same kind of raw image at `org 0x7C00`, pads it to 512 bytes, and writes the signature `0xAA55`. `--flat` and `--boot` cannot be combined. `_start` must be `bits 16 naked fn`: the BIOS enters the first byte in real mode, and a normal function prologue uses 64-bit registers. The output is the sector itself, not an ELF.

`naked` drops the prologue and the epilogue. Parameters are the incoming registers and are not copied: `rdi` `rsi` `rdx` `rcx` `r8` `r9` in 64-bit code (at most 6), `eax` `ecx` `edx` `esi` in `bits 32` (at most 4), and `ax` `cx` `dx` `si` in `bits 16` (at most 4). `ax`/`eax` are the accumulator, so a later expression overwrites that parameter. A scalar or pointer `let` is a labeled slot in the image (`dd` in 16/32-bit code, `dq` in 64-bit code), not a frame, so it survives `ss = 0`. An array or a struct local is still rejected. Inside `naked`, `ax = 0`, `sp = 0x7C00`, `ds = ax`, `ax = ds`, `*(0x1000 as *u32) = 0x2003`, and `*(rbp + 136) = x` are the registers and the store. The same names outside `naked` are an error, unless a real variable has that name. `jmp 0x8200` and `jmp say` are direct jumps. `far_jump(0x18)` reloads `CS` in 64-bit code and continues at the next instruction; `retfq()` is the bare instruction. `return`, and falling off the end, are a bare `ret`. `bits 16` or `bits 32` may be written before or after `naked`. After a 16-bit or 32-bit function the compiler emits `bits 64`, so a later function is 64-bit again. A function that is not marked `bits` emits no `bits` directive.

```copihue
data banner { db "Copihue", 13, 10, 0; }

bits 16 naked fn _start() {
    cli();
    int(0x10);          // assembles as `int 16` (CD 10)
    ljmp(0x08, pm);     // assembles as `jmp 8:pm`
}

bits 32 naked fn pm() { hlt(); }
```

`data` blocks are raw NASM in the text stream: `db`, `dw`, `dd`, `dq`, `times N db|dw|dd|dq value`, `align N` (zero padding), and `label:`. A string becomes its bytes, so a `;` inside the text is data. Integer literals in a data block are passed through as written (`dw 0xFFFF` stays hex). An operand may add or subtract an integer and a label (`dq page + 3`); NASM computes the address. `int` and `ljmp` go through the expression parser, so `0x10` is emitted as decimal `16`; NASM treats that as the same number. `ljmp` takes a selector literal and a label name, not `&label`.

`tests/boot/boot_sector.lap` is a complete sector: BIOS teletype of `Copihue`, then a far jump into 32-bit protected mode that writes `COPIHUE` to COM1. Its GDT is its own (selector `0x08` is 32-bit code, `0x10` is data). That is not the AtomicOS loader GDT, whose 64-bit code selector is `0x18`. That sector still writes CR0 and loads the GDT with `asm`. `tests/boot/realmode_builtins.lap` does the same transition with `lgdt(&gdtr)`, `write_cr0(read_cr0() | 1)`, `if carry`, and `outb` to COM1, and QEMU checks `COPIHUE` there too. `tests/boot/seg_local.lap` stays in real mode, sets `ds` and `es`, and prints `COPIHUE` from a local. `tests/boot/jmp_sp.lap` sets `ds`, `es`, `ss`, and `sp`, jumps to a label, and prints `COPIHUE`. `tests/boot/stc_carry.lap` sets the carry flag and prints `COPIHUE` from that arm. The other two images stop in protected mode. Long mode needs page tables built in memory outside the sector; `bits 64 naked fn` is how that stub would be written. `tests/freestanding/bits32_regs.lap` assembles `write_cr4`, `wrmsr(rdmsr(...) | imm)`, and `cpuid` and is not booted. Uninitialized `let` globals are not placed in the sector: use `data`.

