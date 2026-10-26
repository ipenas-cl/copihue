// Same SPSC ring as spsc.lap. Acquire load, release store, pause.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

static constexpr std::uint64_t CAP = 1024;
static constexpr std::uint64_t N = 1000000;

struct alignas(64) Idx {
    std::atomic<std::uint64_t> v;
};

static Idx write_idx;
static Idx read_idx;
alignas(64) static std::uint64_t buf[CAP];

static void pause_once() {
    asm volatile("pause");
}

static void producer() {
    std::uint64_t w = 0;
    for (std::uint64_t i = 1; i <= N; i++) {
        while (w - read_idx.v.load(std::memory_order_acquire) == CAP) pause_once();
        buf[w & (CAP - 1)] = i;
        w++;
        write_idx.v.store(w, std::memory_order_release);
    }
}

int main() {
    std::thread p(producer);
    std::uint64_t r = 0;
    std::uint64_t sum = 0;
    std::uint64_t got = 0;
    while (got < N) {
        while (r == write_idx.v.load(std::memory_order_acquire)) pause_once();
        sum += buf[r & (CAP - 1)];
        r++;
        read_idx.v.store(r, std::memory_order_release);
        got++;
    }
    p.join();
    std::printf("%llu\n", (unsigned long long)sum);
    return 0;
}
