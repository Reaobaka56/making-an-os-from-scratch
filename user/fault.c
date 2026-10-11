#include <stdio.h>

int main(void) {
    puts("about to dereference NULL...");
    *(volatile int*)0 = 1;
    puts("unreachable");
    return 0;
}
