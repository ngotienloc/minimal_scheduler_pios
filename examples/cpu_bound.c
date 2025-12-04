#include <stdio.h>

int main() {
    volatile unsigned long x = 0;
    for (unsigned long i = 0; i < 1000000000UL; i++)
        x += i;
    printf("CPU-bound task done: %lu\n", x);
    return 0;
}
