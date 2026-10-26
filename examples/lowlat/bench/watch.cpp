/* BENCH: watch BEATS=4000000 QUIET=1000000 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

static constexpr std::uint64_t BEATS = 4000000;
static constexpr std::uint64_t QUIET = 1000000;

static std::atomic<std::uint64_t> hb{0};
static std::atomic<std::uint64_t> phase{0};

static void pause_once() { asm volatile("pause"); }

static void worker() {
    for (std::uint64_t i = 1; i <= BEATS; i++)
        hb.store(i, std::memory_order_release);
    phase.store(1, std::memory_order_release);
    while (phase.load(std::memory_order_acquire) != 2) pause_once();
}

int main() {
    std::uint64_t miss = 0;
    std::uint64_t quiet = 0;
    std::uint64_t cur;
    std::thread t(worker);
    while (phase.load(std::memory_order_acquire) == 0) pause_once();
    cur = hb.load(std::memory_order_acquire);
    while (quiet < QUIET) {
        std::uint64_t c2 = hb.load(std::memory_order_acquire);
        if (c2 == cur) {
            quiet++;
        } else {
            miss++;
            cur = c2;
            quiet = 0;
        }
    }
    phase.store(2, std::memory_order_release);
    t.join();
    std::printf("%llu %llu\n", (unsigned long long)cur, (unsigned long long)miss);
    return 0;
}
