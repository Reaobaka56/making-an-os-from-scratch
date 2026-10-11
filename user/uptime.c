#include <stdio.h>
#include <unistd.h>
int main(void) { printf("  %lu ms\n", uptime_ms()); return 0; }
