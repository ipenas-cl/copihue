/* BENCH: vec N=1000000 */
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    const std::int64_t n = 1000000;
    std::vector<std::int64_t> v;
    for (std::int64_t i = 0; i < n; i++)
        v.push_back(i);
    std::int64_t s = 0;
    for (std::int64_t x : v)
        s += x;
    std::printf("%lld %lld %lld\n",
                static_cast<long long>(s / 1000000),
                static_cast<long long>(s % 1000000),
                static_cast<long long>(v.size()));
    return 0;
}
