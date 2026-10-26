// BENCH: same N=4000000 cpus=0,0
use std::sync::atomic::{AtomicI64, AtomicU64, Ordering};
use std::thread;

const N: u64 = 4_000_000;

#[repr(C, align(64))]
struct Cell {
    v: AtomicU64,
}
#[repr(C)]
struct Line {
    a: Cell,
    b: Cell,
}

static LINE: Line = Line {
    a: Cell { v: AtomicU64::new(0) },
    b: Cell { v: AtomicU64::new(0) },
};
static PA: AtomicI64 = AtomicI64::new(1);
static PB: AtomicI64 = AtomicI64::new(1);

fn pin_cpu(cpu: usize) -> i64 {
    if cpu >= 1024 {
        return -1;
    }
    let mut mask = [0u8; 128];
    mask[cpu / 8] = 1u8 << (cpu % 8);
    let ret: i64;
    unsafe {
        std::arch::asm!(
            "syscall",
            in("rax") 203u64,
            in("rdi") 0u64,
            in("rsi") 128u64,
            in("rdx") mask.as_ptr(),
            lateout("rax") ret,
            lateout("rcx") _,
            lateout("r11") _,
        );
    }
    ret
}

fn main() {
    assert_eq!(std::mem::offset_of!(Line, b), 64);
    let t1 = thread::spawn(|| {
        PA.store(pin_cpu(0), Ordering::Relaxed);
        for _ in 0..N {
            LINE.a.v.fetch_add(1, Ordering::Relaxed);
        }
    });
    let t2 = thread::spawn(|| {
        PB.store(pin_cpu(0), Ordering::Relaxed);
        for _ in 0..N {
            LINE.b.v.fetch_add(1, Ordering::Relaxed);
        }
    });
    t1.join().unwrap();
    t2.join().unwrap();
    let mut s = LINE.a.v.load(Ordering::Relaxed) + LINE.b.v.load(Ordering::Relaxed);
    if PA.load(Ordering::Relaxed) != 0 || PB.load(Ordering::Relaxed) != 0 {
        s = 0;
    }
    println!("{s}");
}
