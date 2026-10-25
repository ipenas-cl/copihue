/* BENCH: parse n=65536 reps=512 */
#include <stdio.h>

unsigned char buf[65536];

int main(void) {
    long n = 65536L;
    long reps = 512L;
    long i = 0;
    long r = 0;
    long c = 0;
    long v = 0;
    long sum = 0;
    while (i < n) {
        if ((i % 10) == 9) {
            buf[i] = 44;
        } else {
            buf[i] = (unsigned char)(48 + (i % 10));
        }
        i = i + 1;
    }
    while (r < reps) {
        i = 0;
        v = 0;
        while (i < n) {
            c = buf[i];
            if (c == 44) {
                sum = sum + v;
                v = 0;
            } else {
                v = v * 10 + (c - 48);
            }
            i = i + 1;
        }
        sum = sum + v;
        r = r + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
