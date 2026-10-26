/* BENCH: same N=4000000 cpus=0,0 */
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <sched.h>
#include <thread>

static constexpr int N = 4000000;

struct alignas(64) Cell {
    std::atomic<std::uint64_t> v;
};
struct Line {
    Cell a;
    Cell b;
};
static_assert(offsetof(Line, b) == 64, "split lines");

static Line line;
static int pa;
static int pb;

static int pin_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (cpu < 0 || cpu >= CPU_SETSIZE) return -1;
    CPU_SET(cpu, &set);
    return sched_setaffinity(0, sizeof(set), &set);
}

static void add_a() {
    int i;
    pa = pin_cpu(0);
    for (i = 0; i < N; i++)
        line.a.v.fetch_add(1, std::memory_order_relaxed);
}
static void add_b() {
    int i;
    pb = pin_cpu(0);
    for (i = 0; i < N; i++)
        line.b.v.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    unsigned long long s;
    std::thread t1(add_a);
    std::thread t2(add_b);
    t1.join();
    t2.join();
    s = line.a.v.load(std::memory_order_relaxed) +
        line.b.v.load(std::memory_order_relaxed);
    if (pa != 0 || pb != 0) s = 0;
    std::printf("%llu\n", s);
    return 0;
}
