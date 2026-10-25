/* BENCH: array n=100000 rounds=1000 */
#include <stdio.h>

int main(void) {
    long n = 100000;
    long rounds = 1000;
    long a[100000];
    long i = 0;
    long v;
    long sum = 0;
    long round = 0;
    while (i < n) {
        a[i] = (i * 3 + 1) & 1023;
        i = i + 1;
    }
    while (round < rounds) {
        i = 0;
        while (i < n) {
            v = a[i];
            a[i] = (v * 3 + (i & 7) + 1) & 1023;
            i = i + 1;
        }
        round = round + 1;
    }
    i = 0;
    while (i < n) {
        sum = sum + a[i] * (i + 1);
        i = i + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
