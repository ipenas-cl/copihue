/* BENCH: pool CAP=1024 REPS=4000 */
#include <stdint.h>
#include <stdio.h>

struct Block {
    struct Block *next;
    uint8_t tag;
    int64_t val;
};

_Static_assert(sizeof(struct Block) == 24, "Block is 24 bytes");

static struct Block slots[1024];
static struct Block *free_head;

static void init(void) {
    int i;
    free_head = 0;
    for (i = 0; i < 1024; i++) {
        slots[i].tag = 0;
        slots[i].val = 0;
        slots[i].next = free_head;
        free_head = &slots[i];
    }
}

static struct Block *alloc_block(void) {
    struct Block *p = free_head;
    if (p == 0) return 0;
    free_head = p->next;
    p->next = 0;
    p->tag = 1;
    return p;
}

static void release(struct Block *p) {
    p->tag = 0;
    p->next = free_head;
    free_head = p;
}

int main(void) {
    int rep;
    int64_t acc = 0;
    init();
    for (rep = 0; rep < 4000; rep++) {
        int i;
        for (i = 0; i < 1024; i++) {
            struct Block *p = alloc_block();
            p->val = (int64_t)i + 1;
        }
        if (alloc_block() == 0) acc += 1;
        for (i = 0; i < 1024; i++) {
            acc += slots[i].val;
            release(&slots[i]);
        }
    }
    printf("%ld\n", (long)(acc & 16777215));
    return 0;
}
