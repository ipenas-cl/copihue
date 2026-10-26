// Same Treiber freelist as pool.lap. CAS is SeqCst.
use std::sync::atomic::{AtomicU64, Ordering};

const CAP: u64 = 4096;
const N: u64 = 1_000_000;
const END: u64 = 4096;

static FREE_HEAD: AtomicU64 = AtomicU64::new(0);
static mut NEXT: [u64; 4096] = [0; 4096];
static mut STORAGE: [u64; 4096] = [0; 4096];

fn alloc_block() -> u64 {
    loop {
        let h = FREE_HEAD.load(Ordering::Acquire);
        if h == END {
            return 0;
        }
        let nxt = unsafe { NEXT[h as usize] };
        if FREE_HEAD
            .compare_exchange(h, nxt, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok()
        {
            return h;
        }
    }
}

fn free_block(h: u64) {
    loop {
        let cur = FREE_HEAD.load(Ordering::Acquire);
        unsafe {
            NEXT[h as usize] = cur;
        }
        if FREE_HEAD
            .compare_exchange(cur, h, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok()
        {
            return;
        }
    }
}

fn main() {
    unsafe {
        let mut i = 0u64;
        while i < CAP - 1 {
            NEXT[i as usize] = i + 1;
            i += 1;
        }
        NEXT[(CAP - 1) as usize] = END;
    }
    FREE_HEAD.store(0, Ordering::Release);
    let mut i = 0u64;
    while i < N {
        let h = alloc_block();
        unsafe {
            STORAGE[h as usize] += 1;
        }
        free_block(h);
        i += 1;
    }
    unsafe {
        println!("{}", STORAGE[0]);
    }
}
