// Same MPMC ring as mpmc.lap. Two producers, two consumers.
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread;

const CAP: u64 = 1024;
const N: u64 = 1_000_000;

#[repr(C, align(64))]
#[derive(Copy, Clone)]
struct Cell {
    seq: u64,
    data: u64,
}

const _: () = assert!(std::mem::size_of::<Cell>() == 64);

static mut CELLS: [Cell; 1024] = [Cell { seq: 0, data: 0 }; 1024];
static ENQ: AtomicU64 = AtomicU64::new(0);
static DEQ: AtomicU64 = AtomicU64::new(0);
static DONE: AtomicU64 = AtomicU64::new(0);
static NEXT_ID: AtomicU64 = AtomicU64::new(0);
static SUMS: [AtomicU64; 2] = [AtomicU64::new(0), AtomicU64::new(0)];

fn pause_once() {
    std::hint::spin_loop();
}

fn seq_ptr(idx: usize) -> *const AtomicU64 {
    unsafe { core::ptr::addr_of!(CELLS[idx].seq) as *const AtomicU64 }
}

fn data_ptr(idx: usize) -> *mut u64 {
    unsafe { core::ptr::addr_of_mut!(CELLS[idx].data) }
}

fn try_push(v: u64) -> bool {
    loop {
        let pos = ENQ.load(Ordering::Acquire);
        let idx = (pos & (CAP - 1)) as usize;
        let seq = unsafe { (*seq_ptr(idx)).load(Ordering::Acquire) };
        if seq != pos {
            if ENQ.load(Ordering::Acquire) == pos {
                return false;
            }
        } else if ENQ
            .compare_exchange(pos, pos + 1, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok()
        {
            unsafe { data_ptr(idx).write_volatile(v) };
            unsafe { (*seq_ptr(idx)).store(pos + 1, Ordering::Release) };
            return true;
        }
    }
}

fn try_pop() -> u64 {
    loop {
        let pos = DEQ.load(Ordering::Acquire);
        let idx = (pos & (CAP - 1)) as usize;
        let seq = unsafe { (*seq_ptr(idx)).load(Ordering::Acquire) };
        if seq != pos + 1 {
            if DEQ.load(Ordering::Acquire) == pos {
                return 0;
            }
        } else if DEQ
            .compare_exchange(pos, pos + 1, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok()
        {
            let v = unsafe { data_ptr(idx).read_volatile() };
            unsafe { (*seq_ptr(idx)).store(pos + CAP, Ordering::Release) };
            return v;
        }
    }
}

fn push_range(lo: u64, hi: u64) {
    let mut i = lo;
    while i <= hi {
        while !try_push(i) {
            pause_once();
        }
        i += 1;
    }
}

fn prod_lo() {
    push_range(1, N / 2);
}

fn prod_hi() {
    push_range(N / 2 + 1, N);
}

fn consumer() {
    let id = NEXT_ID.fetch_add(1, Ordering::SeqCst) as usize;
    let mut sum = 0u64;
    loop {
        let v = try_pop();
        if v != 0 {
            sum += v;
        } else if DONE.load(Ordering::Acquire) != 0 {
            let v2 = try_pop();
            if v2 == 0 {
                SUMS[id].store(sum, Ordering::Release);
                return;
            }
            sum += v2;
        } else {
            pause_once();
        }
    }
}

fn main() {
    unsafe {
        for i in 0..CAP {
            (*seq_ptr(i as usize)).store(i, Ordering::Release);
        }
    }
    let p0 = thread::spawn(prod_lo);
    let p1 = thread::spawn(prod_hi);
    let c0 = thread::spawn(consumer);
    let c1 = thread::spawn(consumer);
    p0.join().unwrap();
    p1.join().unwrap();
    DONE.store(1, Ordering::Release);
    c0.join().unwrap();
    c1.join().unwrap();
    let sum = SUMS[0].load(Ordering::Acquire) + SUMS[1].load(Ordering::Acquire);
    println!("{sum}");
}
