/* @todo: 
step 1:
Add readme or instructions (preferably in markdown)
put most instructions in one place
add more step-by-step (tests, etc)
better instructions for installing
be extremely verbose
goal should be to build with my text file only

step 2:
restructure gen_fib.cpp, use separate functions
make reading instructions clearer
*/

#include <stdio.h>

extern "C" int fib(int);

int main (int argc, char **argv) {
    for (int i = 0; i < 14; ++i) {
        printf("%d ", fib(i));
    }
    
    return 0;
}