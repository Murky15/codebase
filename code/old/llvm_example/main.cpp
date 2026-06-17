#include <stdio.h>

extern "C" int fib(int);

int main (int argc, char **argv) {
    for (int i = 0; i < 14; ++i) {
        printf("%d ", fib(i));
    }
    
    return 0;
}