// Same MPMC ring as mpmc.lap. Two producers, two consumers.
// Both sides claim a ticket with compare_exchange (lock cmpxchg).
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

static constexpr std::uint64_t CAP = 1024;
static constexpr std::uint64_t N = 1000000;

struct alignas(64) Cell {
    std::atomic<std::uint64_t> seq;
    std::uint64_t data;
};
static_assert(sizeof(Cell) == 64, "a cell is one cache line");

static Cell cells[CAP];
alignas(64) static std::atomic<std::uint64_t> enq{0};
alignas(64) static std::atomic<std::uint64_t> deq{0};
alignas(64) static std::atomic<std::uint64_t> done{0};
static std::atomic<std::uint64_t> next_id{0};
static std::uint64_t sums[2];

static void pause_once() { asm volatile("pause"); }

static int try_push(std::uint64_t v) {
    for (;;) {
        std::uint64_t pos = enq.load(std::memory_order_acquire);
        Cell* cell = &cells[pos & (CAP - 1)];
        std::uint64_t seq = cell->seq.load(std::memory_order_acquire);
        if (seq != pos) {
            if (enq.load(std::memory_order_acquire) == pos) return 0;
        } else {
            std::uint64_t expected = pos;
            if (enq.compare_exchange_strong(expected, pos + 1,
                    std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                cell->data = v;
                cell->seq.store(pos + 1, std::memory_order_release);
                return 1;
            }
        }
    }
}

static std::uint64_t try_pop() {
    for (;;) {
        std::uint64_t pos = deq.load(std::memory_order_acquire);
        Cell* cell = &cells[pos & (CAP - 1)];
        std::uint64_t seq = cell->seq.load(std::memory_order_acquire);
        if (seq != pos + 1) {
            if (deq.load(std::memory_order_acquire) == pos) return 0;
        } else {
            std::uint64_t expected = pos;
            if (deq.compare_exchange_strong(expected, pos + 1,
                    std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                std::uint64_t v = cell->data;
                cell->seq.store(pos + CAP, std::memory_order_release);
                return v;
            }
        }
    }
}

static void push_range(std::uint64_t lo, std::uint64_t hi) {
    for (std::uint64_t i = lo; i <= hi; i++) {
        while (try_push(i) == 0) pause_once();
    }
}

static void prod_lo() { push_range(1, N / 2); }
static void prod_hi() { push_range(N / 2 + 1, N); }

static void consumer() {
    std::uint64_t id = next_id.fetch_add(1, std::memory_order_seq_cst);
    std::uint64_t sum = 0;
    for (;;) {
        std::uint64_t v = try_pop();
        if (v != 0) {
            sum += v;
        } else if (done.load(std::memory_order_acquire) != 0) {
            v = try_pop();
            if (v == 0) {
                sums[id] = sum;
                return;
            }
            sum += v;
        } else {
            pause_once();
        }
    }
}

int main() {
    for (std::uint64_t i = 0; i < CAP; i++)
        cells[i].seq.store(i, std::memory_order_release);
    std::thread p0(prod_lo);
    std::thread p1(prod_hi);
    std::thread c0(consumer);
    std::thread c1(consumer);
    p0.join();
    p1.join();
    done.store(1, std::memory_order_release);
    c0.join();
    c1.join();
    std::printf("%llu\n", (unsigned long long)(sums[0] + sums[1]));
    return 0;
}
