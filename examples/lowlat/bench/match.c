/* BENCH: match ORD=256 LEV=32 OPS=600000 */
#include <stdint.h>
#include <stdio.h>

struct Order {
    int32_t id, qty, next;
};
struct Level {
    int32_t price, head, tail;
    uint8_t live;
};

_Static_assert(sizeof(struct Order) == 12, "Order is 12 bytes");
_Static_assert(sizeof(struct Level) == 16, "Level is 16 bytes");

static struct Order orders[256];
static struct Level bids[32];
static struct Level asks[32];
static int32_t free_ord, nbid, nask;
static int64_t filled;
static uint64_t rng = 1;

static void book_init(void) {
    int i;
    for (i = 0; i < 256; i++) {
        orders[i].id = 0;
        orders[i].qty = 0;
        orders[i].next = (int32_t)(i + 1);
    }
    orders[255].next = -1;
    free_ord = 0;
    nbid = 0;
    nask = 0;
}

static int32_t take(void) {
    int32_t oi = free_ord;
    if (oi < 0) return -1;
    free_ord = orders[oi].next;
    return oi;
}

static void give(int32_t oi) {
    orders[oi].id = 0;
    orders[oi].qty = 0;
    orders[oi].next = free_ord;
    free_ord = oi;
}

static int32_t find_level(int64_t side, int64_t price) {
    int64_t n = nbid;
    int64_t i = 0;
    if (side != 0) n = nask;
    while (i < n) {
        if (side == 0) {
            if (bids[i].price == price) return (int32_t)i;
        } else {
            if (asks[i].price == price) return (int32_t)i;
        }
        i++;
    }
    return -1;
}

static int32_t add_level(int64_t side, int64_t price) {
    int64_t n = nbid;
    if (side != 0) n = nask;
    if (n >= 32) return -1;
    if (side == 0) {
        bids[n].price = (int32_t)price;
        bids[n].head = -1;
        bids[n].tail = -1;
        bids[n].live = 1;
        nbid = (int32_t)(n + 1);
    } else {
        asks[n].price = (int32_t)price;
        asks[n].head = -1;
        asks[n].tail = -1;
        asks[n].live = 1;
        nask = (int32_t)(n + 1);
    }
    return (int32_t)n;
}

static void drop_level(int64_t side, int32_t li) {
    int64_t n = nbid;
    int64_t last;
    if (side != 0) n = nask;
    last = n - 1;
    if (li != last) {
        if (side == 0) bids[li] = bids[last];
        else asks[li] = asks[last];
    }
    if (side == 0) nbid = (int32_t)last;
    else nask = (int32_t)last;
}

static int64_t insert(int64_t side, int64_t id, int64_t price, int64_t qty) {
    int32_t li = find_level(side, price);
    int32_t oi;
    if (li < 0) {
        li = add_level(side, price);
        if (li < 0) return 2;
    }
    oi = take();
    if (oi < 0) return 3;
    orders[oi].id = (int32_t)id;
    orders[oi].qty = (int32_t)qty;
    orders[oi].next = -1;
    if (side == 0) {
        if (bids[li].head < 0) {
            bids[li].head = oi;
            bids[li].tail = oi;
        } else {
            orders[bids[li].tail].next = oi;
            bids[li].tail = oi;
        }
    } else {
        if (asks[li].head < 0) {
            asks[li].head = oi;
            asks[li].tail = oi;
        } else {
            orders[asks[li].tail].next = oi;
            asks[li].tail = oi;
        }
    }
    return 0;
}

static int64_t cancel(int64_t id) {
    int64_t side = 0;
    while (side < 2) {
        int64_t n = nbid;
        int64_t li = 0;
        if (side != 0) n = nask;
        while (li < n) {
            int64_t prev = -1;
            int64_t cur = 0;
            if (side == 0) cur = bids[li].head;
            else cur = asks[li].head;
            while (cur >= 0) {
                if (orders[cur].id == id) {
                    int32_t nxt = orders[cur].next;
                    if (prev < 0) {
                        if (side == 0) bids[li].head = nxt;
                        else asks[li].head = nxt;
                    } else {
                        orders[prev].next = nxt;
                    }
                    if (side == 0) {
                        if (bids[li].tail == cur) bids[li].tail = (int32_t)prev;
                        if (bids[li].head < 0) drop_level(0, (int32_t)li);
                    } else {
                        if (asks[li].tail == cur) asks[li].tail = (int32_t)prev;
                        if (asks[li].head < 0) drop_level(1, (int32_t)li);
                    }
                    give((int32_t)cur);
                    return 0;
                }
                prev = cur;
                cur = orders[cur].next;
            }
            li++;
        }
        side++;
    }
    return 1;
}

static int64_t best_bid(void) {
    int64_t i = 0, best = 0, seen = 0;
    while (i < nbid) {
        int64_t px = bids[i].price;
        if (seen == 0 || px > best) {
            best = px;
            seen = 1;
        }
        i++;
    }
    return best;
}

static int64_t best_ask(void) {
    int64_t i = 0, best = 0, seen = 0;
    while (i < nask) {
        int64_t px = asks[i].price;
        if (seen == 0 || px < best) {
            best = px;
            seen = 1;
        }
        i++;
    }
    return best;
}


static int32_t best_bid_li(void) {
    int64_t i = 0, best = 0;
    int32_t at = -1;
    while (i < nbid) {
        int64_t px = bids[i].price;
        if (at < 0 || px > best) { best = px; at = (int32_t)i; }
        i++;
    }
    return at;
}

static int32_t best_ask_li(void) {
    int64_t i = 0, best = 0;
    int32_t at = -1;
    while (i < nask) {
        int64_t px = asks[i].price;
        if (at < 0 || px < best) { best = px; at = (int32_t)i; }
        i++;
    }
    return at;
}

static int64_t cross(int64_t is_buy, int64_t limit_px, int64_t qty, int64_t is_market, int64_t taker) {
    int64_t left = qty;
    int64_t go = 1;
    (void)taker;
    while (left > 0 && go == 1) {
        int32_t li;
        int64_t px;
        int32_t oi;
        if (is_buy != 0) li = best_ask_li();
        else li = best_bid_li();
        if (li < 0) {
            go = 0;
        } else {
            if (is_buy != 0) { px = asks[li].price; oi = asks[li].head; }
            else { px = bids[li].price; oi = bids[li].head; }
            if (is_market == 0) {
                if (is_buy != 0 && px > limit_px) go = 0;
                if (is_buy == 0 && px < limit_px) go = 0;
            }
            if (go == 1) {
                int64_t avail = orders[oi].qty;
                int64_t takeq = left;
                int64_t maker = orders[oi].id;
                if (takeq > avail) takeq = avail;
                filled += takeq;
                orders[oi].qty = (int32_t)(avail - takeq);
                left -= takeq;
                if (orders[oi].qty == 0) cancel(maker);
            }
        }
    }
    return left;
}

static int64_t limit_order(int64_t side, int64_t id, int64_t price, int64_t qty) {
    int64_t is_buy = 0;
    int64_t left;
    if (side == 0) is_buy = 1;
    left = cross(is_buy, price, qty, 0, id);
    if (left > 0) insert(side, id, price, left);
    return left;
}

static int64_t market_order(int64_t side, int64_t id, int64_t qty) {
    int64_t is_buy = 0;
    int64_t left;
    if (side == 0) is_buy = 1;
    left = cross(is_buy, 0, qty, 1, id);
    return left;
}

static int64_t rnd(void) {
    rng = rng * 1103515245ull + 12345ull;
    return (int64_t)((rng >> 16) & 32767ull);
}

int main(void) {
    int64_t acc = 0;
    int64_t n = 0;
    int64_t id = 1;
    book_init();
    while (n < 600000) {
        int64_t op = rnd() % 10;
        if (op < 5) {
            int64_t side = rnd() & 1;
            int64_t px = (rnd() % 32) + 1;
            int64_t qty = (rnd() % 4) + 1;
            acc += limit_order(side, id, px, qty);
        } else if (op < 8) {
            int64_t side = rnd() & 1;
            int64_t qty = (rnd() % 4) + 1;
            acc += market_order(side, id, qty);
        } else if (op < 9) {
            int64_t which = 1;
            if (id > 1) which = (rnd() % (id - 1)) + 1;
            acc += cancel(which);
        } else {
            acc += best_bid() + best_ask();
        }
        id += 1;
        n += 1;
    }
    printf("%ld\n", (long)((acc + filled) & 16777215));
    return 0;
}
