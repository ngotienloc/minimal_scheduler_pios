// cpu_bound.c
#include <stdio.h>
int main(void){
    volatile unsigned long x = 0;
    for (unsigned long i = 0; i < 500000000UL; ++i) x += i;
    printf("done %lu\n", x);
    return 0;
}
