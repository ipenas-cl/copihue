/* Calls into an object written by `copihue -c` (tests/link/parts/mod_add.lap).
 * tests/x64/run_obj.sh links it with gcc and expects "42 7". */
#include <stdio.h>

extern long add(long a, long b);
extern long bias;

int main(void) {
    printf("%ld %ld\n", add(35, bias), bias);
    return 0;
}
