/* BENCH: chain n=80000000 */
#include <stdio.h>

int main(void) {
    long n = 80000000L;
    long x = 1;
    long i = 0;
    while (i < n) {
        x = (x + i) & 2147483647L;
        x = (x * 13) & 2147483647L;
        x = x ^ (x >> 7);
        i = i + 1;
    }
    printf("%ld\n", x);
    return 0;
}
