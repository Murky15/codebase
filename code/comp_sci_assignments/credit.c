#include <stdint.h>
#include <stdio.h>

int
main (void) {
    uint64_t num;
    printf("Number: ");
    scanf("%llu", &num);
    return 0;
}