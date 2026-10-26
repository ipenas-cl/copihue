// Same SPMC ring as spmc.lap. One producer, three consumers.
// The producer keeps a private cursor. Consumers CAS the dequeue index.
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
alignas(64) static std::atomic<std::uint64_t> deq{0};
alignas(64) static std::atomic<std::uint64_t> done{0};
static std::atomic<std::uint64_t> next_id{0};
static std::uint64_t sums[3];

static void pause_once() { asm volatile("pause"); }

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

static void producer() {
    std::uint64_t pos = 0;
    for (std::uint64_t i = 1; i <= N; i++) {
        Cell* cell = &cells[pos & (CAP - 1)];
        while (cell->seq.load(std::memory_order_acquire) != pos) pause_once();
        cell->data = i;
        cell->seq.store(pos + 1, std::memory_order_release);
        pos++;
    }
}

int main() {
    for (std::uint64_t i = 0; i < CAP; i++)
        cells[i].seq.store(i, std::memory_order_release);
    std::thread p(producer);
    std::thread c1(consumer);
    std::thread c2(consumer);
    std::thread c3(consumer);
    p.join();
    done.store(1, std::memory_order_release);
    c1.join();
    c2.join();
    c3.join();
    std::printf("%llu\n", (unsigned long long)(sums[0] + sums[1] + sums[2]));
    return 0;
}
