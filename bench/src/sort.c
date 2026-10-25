/* BENCH: sort n=28000 */
#include <stdio.h>

int main(void) {
    long n = 28000;
    long a[28000];
    long i = 0;
    long j;
    long key;
    long sum = 0;
    while (i < n) {
        a[i] = (i * 97 + 13) & 4095;
        i = i + 1;
    }
    i = 1;
    while (i < n) {
        key = a[i];
        j = i - 1;
        while (j >= 0 && a[j] > key) {
            a[j + 1] = a[j];
            j = j - 1;
        }
        a[j + 1] = key;
        i = i + 1;
    }
    i = 0;
    while (i < n) {
        sum = sum + a[i] * (i + 1);
        i = i + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
