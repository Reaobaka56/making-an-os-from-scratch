#include <stdio.h>
#include <unistd.h>
int main(int argc, char** argv) {
    int rc = 0;
    for (int i = 1; i < argc; i++) if (unlink(argv[i]) < 0) { printf("rm: %s: no such file\n", argv[i]); rc = 1; }
    if (argc < 2) { fputs("usage: rm <file>...\n", stderr); return 1; }
    return rc;
}
