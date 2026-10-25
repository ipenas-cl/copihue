/* BENCH: fill n=262144 reps=128 */
#include <stdio.h>

unsigned char buf[262144];

int main(void) {
    long n = 262144L;
    long reps = 128L;
    long i = 0;
    long r = 0;
    long sum = 0;
    while (r < reps) {
        i = 0;
        while (i < n) {
            buf[i] = (unsigned char)((buf[i] + i + r) & 255);
            i = i + 1;
        }
        i = 0;
        while (i < n) {
            sum = sum + buf[i];
            i = i + 1;
        }
        r = r + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
