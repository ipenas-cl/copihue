// BENCH: match ORD=256 LEV=32 OPS=600000
#[repr(C)]
#[derive(Copy, Clone)]
struct Order {
    id: i32,
    qty: i32,
    next: i32,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct Level {
    price: i32,
    head: i32,
    tail: i32,
    live: u8,
}

const _: () = assert!(std::mem::size_of::<Order>() == 12);
const _: () = assert!(std::mem::size_of::<Level>() == 16);

struct Book {
    orders: [Order; 256],
    bids: [Level; 32],
    asks: [Level; 32],
    free_ord: i32,
    nbid: i32,
    nask: i32,
    filled: i64,
}

fn book_init(b: &mut Book) {
    let mut i = 0;
    while i < 256 {
        b.orders[i].id = 0;
        b.orders[i].qty = 0;
        b.orders[i].next = (i + 1) as i32;
        i += 1;
    }
    b.orders[255].next = -1;
    b.free_ord = 0;
    b.nbid = 0;
    b.nask = 0;
    b.filled = 0;
}

fn take(b: &mut Book) -> i32 {
    let oi = b.free_ord;
    if oi < 0 {
        return -1;
    }
    b.free_ord = b.orders[oi as usize].next;
    oi
}

fn give(b: &mut Book, oi: i32) {
    b.orders[oi as usize].id = 0;
    b.orders[oi as usize].qty = 0;
    b.orders[oi as usize].next = b.free_ord;
    b.free_ord = oi;
}

fn find_level(b: &Book, side: i64, price: i64) -> i32 {
    let mut n = b.nbid as i64;
    let mut i = 0i64;
    if side != 0 {
        n = b.nask as i64;
    }
    while i < n {
        if side == 0 {
            if b.bids[i as usize].price as i64 == price {
                return i as i32;
            }
        } else if b.asks[i as usize].price as i64 == price {
            return i as i32;
        }
        i += 1;
    }
    -1
}

fn add_level(b: &mut Book, side: i64, price: i64) -> i32 {
    let mut n = b.nbid as i64;
    if side != 0 {
        n = b.nask as i64;
    }
    if n >= 32 {
        return -1;
    }
    if side == 0 {
        b.bids[n as usize].price = price as i32;
        b.bids[n as usize].head = -1;
        b.bids[n as usize].tail = -1;
        b.bids[n as usize].live = 1;
        b.nbid = (n + 1) as i32;
    } else {
        b.asks[n as usize].price = price as i32;
        b.asks[n as usize].head = -1;
        b.asks[n as usize].tail = -1;
        b.asks[n as usize].live = 1;
        b.nask = (n + 1) as i32;
    }
    n as i32
}

fn drop_level(b: &mut Book, side: i64, li: i32) {
    let mut n = b.nbid as i64;
    if side != 0 {
        n = b.nask as i64;
    }
    let last = n - 1;
    if li as i64 != last {
        if side == 0 {
            b.bids[li as usize] = b.bids[last as usize];
        } else {
            b.asks[li as usize] = b.asks[last as usize];
        }
    }
    if side == 0 {
        b.nbid = last as i32;
    } else {
        b.nask = last as i32;
    }
}

fn insert(b: &mut Book, side: i64, id: i64, price: i64, qty: i64) -> i64 {
    let mut li = find_level(b, side, price);
    if li < 0 {
        li = add_level(b, side, price);
        if li < 0 {
            return 2;
        }
    }
    let oi = take(b);
    if oi < 0 {
        return 3;
    }
    b.orders[oi as usize].id = id as i32;
    b.orders[oi as usize].qty = qty as i32;
    b.orders[oi as usize].next = -1;
    if side == 0 {
        if b.bids[li as usize].head < 0 {
            b.bids[li as usize].head = oi;
            b.bids[li as usize].tail = oi;
        } else {
            let tail = b.bids[li as usize].tail as usize;
            b.orders[tail].next = oi;
            b.bids[li as usize].tail = oi;
        }
    } else if b.asks[li as usize].head < 0 {
        b.asks[li as usize].head = oi;
        b.asks[li as usize].tail = oi;
    } else {
        let tail = b.asks[li as usize].tail as usize;
        b.orders[tail].next = oi;
        b.asks[li as usize].tail = oi;
    }
    0
}

fn cancel(b: &mut Book, id: i64) -> i64 {
    let mut side = 0i64;
    while side < 2 {
        let mut n = b.nbid as i64;
        let mut li = 0i64;
        if side != 0 {
            n = b.nask as i64;
        }
        while li < n {
            let mut prev = -1i64;
            let mut cur = if side == 0 {
                b.bids[li as usize].head as i64
            } else {
                b.asks[li as usize].head as i64
            };
            while cur >= 0 {
                if b.orders[cur as usize].id as i64 == id {
                    let nxt = b.orders[cur as usize].next;
                    if prev < 0 {
                        if side == 0 {
                            b.bids[li as usize].head = nxt;
                        } else {
                            b.asks[li as usize].head = nxt;
                        }
                    } else {
                        b.orders[prev as usize].next = nxt;
                    }
                    if side == 0 {
                        if b.bids[li as usize].tail as i64 == cur {
                            b.bids[li as usize].tail = prev as i32;
                        }
                        if b.bids[li as usize].head < 0 {
                            drop_level(b, 0, li as i32);
                        }
                    } else {
                        if b.asks[li as usize].tail as i64 == cur {
                            b.asks[li as usize].tail = prev as i32;
                        }
                        if b.asks[li as usize].head < 0 {
                            drop_level(b, 1, li as i32);
                        }
                    }
                    give(b, cur as i32);
                    return 0;
                }
                prev = cur;
                cur = b.orders[cur as usize].next as i64;
                }
            li += 1;
        }
        side += 1;
    }
    1
}

fn best_bid(b: &Book) -> i64 {
    let mut i = 0i64;
    let mut best = 0i64;
    let mut seen = 0i64;
    while i < b.nbid as i64 {
        let px = b.bids[i as usize].price as i64;
        if seen == 0 || px > best {
            best = px;
            seen = 1;
        }
        i += 1;
    }
    best
}

fn best_ask(b: &Book) -> i64 {
    let mut i = 0i64;
    let mut best = 0i64;
    let mut seen = 0i64;
    while i < b.nask as i64 {
        let px = b.asks[i as usize].price as i64;
        if seen == 0 || px < best {
            best = px;
            seen = 1;
        }
        i += 1;
    }
    best
}


fn best_bid_li(b: &Book) -> i32 {
    let mut i = 0i64;
    let mut best = 0i64;
    let mut at: i32 = -1;
    while i < b.nbid as i64 {
        let px = b.bids[i as usize].price as i64;
        if at < 0 || px > best {
            best = px;
            at = i as i32;
        }
        i += 1;
    }
    at
}

fn best_ask_li(b: &Book) -> i32 {
    let mut i = 0i64;
    let mut best = 0i64;
    let mut at: i32 = -1;
    while i < b.nask as i64 {
        let px = b.asks[i as usize].price as i64;
        if at < 0 || px < best {
            best = px;
            at = i as i32;
        }
        i += 1;
    }
    at
}

fn cross(b: &mut Book, is_buy: i64, limit_px: i64, qty: i64, is_market: i64, _taker: i64) -> i64 {
    let mut left = qty;
    let mut go = 1i64;
    while left > 0 && go == 1 {
        let li = if is_buy != 0 { best_ask_li(b) } else { best_bid_li(b) };
        if li < 0 {
            go = 0;
        } else {
            let px;
            let oi;
            if is_buy != 0 {
                px = b.asks[li as usize].price as i64;
                oi = b.asks[li as usize].head;
            } else {
                px = b.bids[li as usize].price as i64;
                oi = b.bids[li as usize].head;
            }
            if is_market == 0 {
                if is_buy != 0 && px > limit_px { go = 0; }
                if is_buy == 0 && px < limit_px { go = 0; }
            }
            if go == 1 {
                let avail = b.orders[oi as usize].qty as i64;
                let mut takeq = left;
                let maker = b.orders[oi as usize].id as i64;
                if takeq > avail { takeq = avail; }
                b.filled += takeq;
                b.orders[oi as usize].qty = (avail - takeq) as i32;
                left -= takeq;
                if b.orders[oi as usize].qty == 0 { cancel(b, maker); }
            }
        }
    }
    left
}

fn limit_order(b: &mut Book, side: i64, id: i64, price: i64, qty: i64) -> i64 {
    let is_buy = if side == 0 { 1 } else { 0 };
    let left = cross(b, is_buy, price, qty, 0, id);
    if left > 0 { insert(b, side, id, price, left); }
    left
}

fn market_order(b: &mut Book, side: i64, id: i64, qty: i64) -> i64 {
    let is_buy = if side == 0 { 1 } else { 0 };
    cross(b, is_buy, 0, qty, 1, id)
}

fn main() {
    let mut b = Book {
        orders: [Order { id: 0, qty: 0, next: 0 }; 256],
        bids: [Level { price: 0, head: 0, tail: 0, live: 0 }; 32],
        asks: [Level { price: 0, head: 0, tail: 0, live: 0 }; 32],
        free_ord: 0,
        nbid: 0,
        nask: 0,
        filled: 0,
    };
    let mut rng: u64 = 1;
    let mut rnd = || {
        rng = rng.wrapping_mul(1103515245).wrapping_add(12345);
        ((rng >> 16) & 32767) as i64
    };
    book_init(&mut b);
    let mut acc = 0i64;
    let mut n = 0i64;
    let mut id = 1i64;
    while n < 600000 {
        let op = rnd() % 10;
        if op < 5 {
            let side = rnd() & 1;
            let px = (rnd() % 32) + 1;
            let qty = (rnd() % 4) + 1;
            acc += limit_order(&mut b, side, id, px, qty);
        } else if op < 8 {
            let side = rnd() & 1;
            let qty = (rnd() % 4) + 1;
            acc += market_order(&mut b, side, id, qty);
        } else if op < 9 {
            let mut which = 1i64;
            if id > 1 {
                which = (rnd() % (id - 1)) + 1;
            }
            acc += cancel(&mut b, which);
        } else {
            acc += best_bid(&b) + best_ask(&b);
        }
        id += 1;
        n += 1;
    }
    println!("{}", (acc + b.filled) & 16777215);
}
