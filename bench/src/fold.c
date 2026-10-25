/* BENCH: fold n=500000000 */
#include <stdio.h>

int main(void) {
    long n = 500000000L;
    long s = 0;
    long i = 0;
    while (i < n) {
        s = s + i * 3 + 1;
        i = i + 1;
    }
    printf("%ld\n", s);
    return 0;
}
