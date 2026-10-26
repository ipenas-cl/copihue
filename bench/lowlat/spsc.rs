// Same SPSC ring as spsc.lap.
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread;

const CAP: u64 = 1024;
const N: u64 = 1_000_000;

#[repr(align(64))]
struct Idx(AtomicU64);

static WRITE_IDX: Idx = Idx(AtomicU64::new(0));
static READ_IDX: Idx = Idx(AtomicU64::new(0));
static mut BUF: [u64; 1024] = [0; 1024];

fn pause_once() {
    std::hint::spin_loop();
}

fn producer() {
    let mut w = 0u64;
    let mut i = 1u64;
    while i <= N {
        while w - READ_IDX.0.load(Ordering::Acquire) == CAP {
            pause_once();
        }
        unsafe {
            BUF[(w & (CAP - 1)) as usize] = i;
        }
        w += 1;
        WRITE_IDX.0.store(w, Ordering::Release);
        i += 1;
    }
}

fn main() {
    let p = thread::spawn(producer);
    let mut r = 0u64;
    let mut sum = 0u64;
    let mut got = 0u64;
    while got < N {
        let wr = WRITE_IDX.0.load(Ordering::Acquire);
        if r == wr {
            pause_once();
        } else {
            unsafe {
                sum += BUF[(r & (CAP - 1)) as usize];
            }
            r += 1;
            READ_IDX.0.store(r, Ordering::Release);
            got += 1;
        }
    }
    p.join().unwrap();
    println!("{sum}");
}
