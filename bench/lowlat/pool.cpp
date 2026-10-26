// Same Treiber freelist as pool.lap. CAS is seq_cst (lock cmpxchg on x86).
#include <atomic>
#include <cstdint>
#include <cstdio>

static constexpr std::uint64_t CAP = 4096;
static constexpr std::uint64_t N = 1000000;
static constexpr std::uint64_t END = 4096;

static std::atomic<std::uint64_t> free_head{0};
static std::uint64_t next_idx[CAP];
static std::uint64_t storage[CAP];

static std::uint64_t alloc_block() {
    for (;;) {
        std::uint64_t h = free_head.load(std::memory_order_acquire);
        if (h == END) return 0;
        std::uint64_t nxt = next_idx[h];
        if (free_head.compare_exchange_strong(h, nxt, std::memory_order_seq_cst)) return h;
    }
}

static void free_block(std::uint64_t h) {
    for (;;) {
        std::uint64_t cur = free_head.load(std::memory_order_acquire);
        next_idx[h] = cur;
        if (free_head.compare_exchange_strong(cur, h, std::memory_order_seq_cst)) return;
    }
}

int main() {
    for (std::uint64_t i = 0; i < CAP - 1; i++) next_idx[i] = i + 1;
    next_idx[CAP - 1] = END;
    free_head.store(0, std::memory_order_release);
    for (std::uint64_t i = 0; i < N; i++) {
        std::uint64_t h = alloc_block();
        storage[h] += 1;
        free_block(h);
    }
    std::printf("%llu\n", (unsigned long long)storage[0]);
    return 0;
}
