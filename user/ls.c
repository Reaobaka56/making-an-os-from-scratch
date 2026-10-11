#include <stdio.h>
#include <unistd.h>
int main(void) {
    DirInfo d;
    for (int slot = 0;; slot++) {
        int r = readdir(slot, &d);
        if (r < 0) break;
        if (r == 0) printf("  %-24s %6u bytes\n", d.name, d.size);
    }
    return 0;
}
