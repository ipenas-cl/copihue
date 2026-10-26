// BENCH: share N=4000000 line=64
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread;

const N: u64 = 4_000_000;

#[repr(C, align(64))]
struct Line {
    a: AtomicU64,
    b: AtomicU64,
}

static LINE: Line = Line {
    a: AtomicU64::new(0),
    b: AtomicU64::new(0),
};

fn main() {
    assert_eq!(std::mem::size_of::<Line>(), 64);
    assert_eq!(std::mem::offset_of!(Line, b), 8);
    let t1 = thread::spawn(|| {
        for _ in 0..N {
            LINE.a.fetch_add(1, Ordering::Relaxed);
        }
    });
    let t2 = thread::spawn(|| {
        for _ in 0..N {
            LINE.b.fetch_add(1, Ordering::Relaxed);
        }
    });
    t1.join().unwrap();
    t2.join().unwrap();
    println!(
        "{}",
        LINE.a.load(Ordering::Relaxed) + LINE.b.load(Ordering::Relaxed)
    );
}
