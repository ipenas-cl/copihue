/* BENCH: share N=4000000 line=64 */
#include <cstddef>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

static constexpr int N = 4000000;

struct alignas(64) Line {
    std::atomic<std::uint64_t> a;
    std::atomic<std::uint64_t> b;
};
static_assert(sizeof(Line) == 64, "one cache line");
static_assert(offsetof(Line, b) == 8, "b shares the line");

static Line line;

static void add_a() {
    for (int i = 0; i < N; i++)
        line.a.fetch_add(1, std::memory_order_relaxed);
}
static void add_b() {
    for (int i = 0; i < N; i++)
        line.b.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::thread t1(add_a);
    std::thread t2(add_b);
    t1.join();
    t2.join();
    std::printf("%llu\n", (unsigned long long)(
        line.a.load(std::memory_order_relaxed) +
        line.b.load(std::memory_order_relaxed)));
    return 0;
}
