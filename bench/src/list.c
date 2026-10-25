/* BENCH: list n=32768 reps=512 */
#include <stdio.h>

struct Node { long val; long next; };
struct Node nodes[32768];

int main(void) {
    long n = 32768L;
    long reps = 512L;
    long i = 0;
    long last = 0;
    long r = 0;
    long sum = 0;
    last = n - 1;
    while (i < last) {
        nodes[i].val = i;
        nodes[i].next = i + 1;
        i = i + 1;
    }
    nodes[last].val = last;
    nodes[last].next = -1;
    while (r < reps) {
        i = 0;
        while (i >= 0) {
            sum = sum + nodes[i].val;
            i = nodes[i].next;
        }
        r = r + 1;
    }
    printf("%ld\n", sum);
    return 0;
}
