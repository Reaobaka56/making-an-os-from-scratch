#include <stdio.h>
#include <unistd.h>

static void copy(int fd) {
    char buf[256]; long n;
    while ((n = read(fd, buf, sizeof buf)) > 0) write(1, buf, n);
}

int main(int argc, char** argv) {
    if (argc < 2) { copy(0); return 0; }               // no files: copy stdin (pipes, redirects, ctrl-D to stop)
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) { printf("cat: %s: no such file\n", argv[i]); rc = 1; continue; }
        char buf[256]; long n; char last = '\n';
        while ((n = read(fd, buf, sizeof buf)) > 0) { write(1, buf, n); last = buf[n - 1]; }
        if (last != '\n') putchar('\n');
        close(fd);
    }
    return rc;
}
