// BENCH: split N=4000000 line=128
use std::sync::atomic::{AtomicU64, Ordering};
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

fn main() {
    assert_eq!(std::mem::size_of::<Cell>(), 64);
    assert_eq!(std::mem::offset_of!(Line, b), 64);
    let t1 = thread::spawn(|| {
        for _ in 0..N {
            LINE.a.v.fetch_add(1, Ordering::Relaxed);
        }
    });
    let t2 = thread::spawn(|| {
        for _ in 0..N {
            LINE.b.v.fetch_add(1, Ordering::Relaxed);
        }
    });
    t1.join().unwrap();
    t2.join().unwrap();
    println!(
        "{}",
        LINE.a.v.load(Ordering::Relaxed) + LINE.b.v.load(Ordering::Relaxed)
    );
}
