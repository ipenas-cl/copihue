/* BENCH: copy n=262144 reps=192 */
#include <stdio.h>

unsigned char src[262144];
unsigned char dst[262144];

int main(void) {
    long n = 262144L;
    long reps = 192L;
    long i = 0;
    long r = 0;
    long sum = 0;
    while (i < n) {
        src[i] = (unsigned char)(i & 255);
        i = i + 1;
    }
    while (r < reps) {
        i = 0;
        while (i < n) {
            dst[i] = src[i];
            i = i + 1;
        }
        i = 0;
        while (i < n) {
            sum = sum + dst[i];
            src[i] = (unsigned char)((src[i] + 1) & 255);
            i = i + 1;
        }
        r = r + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
