// BENCH: pool CAP=1024 REPS=4000
#[repr(C)]
#[derive(Copy, Clone)]
struct Block {
    next: *mut Block,
    tag: u8,
    val: i64,
}

const _: () = assert!(std::mem::size_of::<Block>() == 24);

static mut SLOTS: [Block; 1024] = [Block {
    next: std::ptr::null_mut(),
    tag: 0,
    val: 0,
}; 1024];
static mut FREE: *mut Block = std::ptr::null_mut();

fn init() {
    unsafe {
        FREE = std::ptr::null_mut();
        let mut i = 0usize;
        while i < 1024 {
            SLOTS[i].tag = 0;
            SLOTS[i].val = 0;
            SLOTS[i].next = FREE;
            FREE = &mut SLOTS[i];
            i += 1;
        }
    }
}

fn alloc_block() -> *mut Block {
    unsafe {
        let p = FREE;
        if p.is_null() {
            return std::ptr::null_mut();
        }
        FREE = (*p).next;
        (*p).next = std::ptr::null_mut();
        (*p).tag = 1;
        p
    }
}

fn release(p: *mut Block) {
    unsafe {
        (*p).tag = 0;
        (*p).next = FREE;
        FREE = p;
    }
}

fn main() {
    let mut rep = 0;
    let mut acc: i64 = 0;
    init();
    while rep < 4000 {
        let mut i: i64 = 0;
        while i < 1024 {
            let p = alloc_block();
            unsafe {
                (*p).val = i + 1;
            }
            i += 1;
        }
        if alloc_block().is_null() {
            acc += 1;
        }
        i = 0;
        while i < 1024 {
            unsafe {
                acc += SLOTS[i as usize].val;
                release(&mut SLOTS[i as usize]);
            }
            i += 1;
        }
        rep += 1;
    }
    println!("{}", acc & 16777215);
}
