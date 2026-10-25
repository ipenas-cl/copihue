/* BENCH: sieve limit=400000 reps=80 */
#include <stdio.h>

int main(void) {
    long limit = 400000;
    long reps = 80;
    unsigned char flags[400000];
    long r = 0;
    long sum = 0;
    while (r < reps) {
        long i = 0;
        long j;
        while (i < limit) {
            flags[i] = 1;
            i = i + 1;
        }
        flags[0] = 0;
        flags[1] = 0;
        i = 2;
        while (i * i < limit) {
            if (flags[i] != 0) {
                j = i * i;
                while (j < limit) {
                    flags[j] = 0;
                    j = j + i;
                }
            }
            i = i + 1;
        }
        sum = 0;
        i = 2;
        while (i < limit) {
            if (flags[i] != 0) {
                sum = sum + i;
            }
            i = i + 1;
        }
        r = r + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
