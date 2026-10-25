/* BENCH: matmul N=384 */
#include <stdio.h>

int main(void) {
    long n = 384;
    long a[147456];
    long b[147456];
    long c[147456];
    long i = 0;
    long j;
    long k;
    long s;
    long sum = 0;
    while (i < n) {
        j = 0;
        while (j < n) {
            a[i * n + j] = (i + j) & 7;
            b[i * n + j] = (i * 3 + j) & 7;
            c[i * n + j] = 0;
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while (i < n) {
        j = 0;
        while (j < n) {
            s = 0;
            k = 0;
            while (k < n) {
                s = s + a[i * n + k] * b[k * n + j];
                k = k + 1;
            }
            c[i * n + j] = s;
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while (i < n * n) {
        sum = sum + c[i];
        i = i + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
