#include "sys.h"

void _start(void) {
    print("about to dereference NULL...\n");
    *(volatile int*)0 = 1;
    print("unreachable\n");
    sys_exit(0);
}
