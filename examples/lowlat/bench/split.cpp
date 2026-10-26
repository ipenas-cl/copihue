/* BENCH: split N=4000000 line=128 */
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>

static constexpr int N = 4000000;

struct alignas(64) Cell {
    std::atomic<std::uint64_t> v;
};
struct Line {
    Cell a;
    Cell b;
};
static_assert(sizeof(Cell) == 64, "cell is a line");
static_assert(offsetof(Line, b) == 64, "b is the next line");

static Line line;

static void add_a() {
    for (int i = 0; i < N; i++)
        line.a.v.fetch_add(1, std::memory_order_relaxed);
}
static void add_b() {
    for (int i = 0; i < N; i++)
        line.b.v.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::thread t1(add_a);
    std::thread t2(add_b);
    t1.join();
    t2.join();
    std::printf("%llu\n", (unsigned long long)(
        line.a.v.load(std::memory_order_relaxed) +
        line.b.v.load(std::memory_order_relaxed)));
    return 0;
}
