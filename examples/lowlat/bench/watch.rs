// BENCH: watch BEATS=4000000 QUIET=1000000
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread;

const BEATS: u64 = 4_000_000;
const QUIET: u64 = 1_000_000;

static HB: AtomicU64 = AtomicU64::new(0);
static PHASE: AtomicU64 = AtomicU64::new(0);

fn pause_once() {
    unsafe { std::arch::asm!("pause") }
}

fn main() {
    let t = thread::spawn(|| {
        let mut i = 1u64;
        while i <= BEATS {
            HB.store(i, Ordering::Release);
            i += 1;
        }
        PHASE.store(1, Ordering::Release);
        while PHASE.load(Ordering::Acquire) != 2 {
            pause_once();
        }
    });
    while PHASE.load(Ordering::Acquire) == 0 {
        pause_once();
    }
    let mut cur = HB.load(Ordering::Acquire);
    let mut miss = 0u64;
    let mut quiet = 0u64;
    while quiet < QUIET {
        let c2 = HB.load(Ordering::Acquire);
        if c2 == cur {
            quiet += 1;
        } else {
            miss += 1;
            cur = c2;
            quiet = 0;
        }
    }
    PHASE.store(2, Ordering::Release);
    t.join().unwrap();
    println!("{cur} {miss}");
}
