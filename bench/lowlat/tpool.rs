// Same thread pool as tpool.lap. Main submits, three workers pop.
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
static DEQ: AtomicU64 = AtomicU64::new(0);
static DONE: AtomicU64 = AtomicU64::new(0);
static NEXT_ID: AtomicU64 = AtomicU64::new(0);
static SUMS: [AtomicU64; 3] = [AtomicU64::new(0), AtomicU64::new(0), AtomicU64::new(0)];

fn pause_once() {
    std::hint::spin_loop();
}

fn seq_ptr(idx: usize) -> *const AtomicU64 {
    unsafe { core::ptr::addr_of!(CELLS[idx].seq) as *const AtomicU64 }
}

fn data_ptr(idx: usize) -> *mut u64 {
    unsafe { core::ptr::addr_of_mut!(CELLS[idx].data) }
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

fn worker() {
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
    let w1 = thread::spawn(worker);
    let w2 = thread::spawn(worker);
    let w3 = thread::spawn(worker);
    let mut pos = 0u64;
    let mut i = 1u64;
    while i <= N {
        let idx = (pos & (CAP - 1)) as usize;
        while unsafe { (*seq_ptr(idx)).load(Ordering::Acquire) } != pos {
            pause_once();
        }
        unsafe { data_ptr(idx).write_volatile(i) };
        unsafe { (*seq_ptr(idx)).store(pos + 1, Ordering::Release) };
        pos += 1;
        i += 1;
    }
    DONE.store(1, Ordering::Release);
    w1.join().unwrap();
    w2.join().unwrap();
    w3.join().unwrap();
    let sum = SUMS[0].load(Ordering::Acquire)
        + SUMS[1].load(Ordering::Acquire)
        + SUMS[2].load(Ordering::Acquire);
    println!("{sum}");
}
