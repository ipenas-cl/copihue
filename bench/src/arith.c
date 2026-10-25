/* BENCH: arith n=100000000 */
#include <stdio.h>

int main(void) {
    long n = 100000000L;
    long a = 1;
    long b = 2;
    long c = 3;
    long i = 0;
    while (i < n) {
        a = (a + b) & 2147483647L;
        b = (b + c) & 2147483647L;
        c = (c + i) & 2147483647L;
        i = i + 1;
    }
    printf("%ld\n", a + b + c);
    return 0;
}
