/* BENCH: hash n=131072 reps=256 */
#include <stdio.h>

unsigned char buf[131072];

int main(void) {
    long n = 131072L;
    long reps = 256L;
    long i = 0;
    long r = 0;
    long h = 0;
    long total = 0;
    while (i < n) {
        buf[i] = (unsigned char)(i & 255);
        i = i + 1;
    }
    while (r < reps) {
        h = 18652613L;
        i = 0;
        while (i < n) {
            h = (h * 16777619L + buf[i]) & 2147483647L;
            i = i + 1;
        }
        total = total + h;
        r = r + 1;
    }
    printf("%ld\n", total);
    return 0;
}
