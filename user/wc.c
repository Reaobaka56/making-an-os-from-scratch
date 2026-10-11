#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

static void count(int fd, const char* name) {
    char buf[256]; long n, lines = 0, words = 0, bytes = 0; int in_word = 0;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        for (long i = 0; i < n; i++) {
            if (buf[i] == '\n') lines++;
            if (isspace((unsigned char)buf[i])) in_word = 0; else if (!in_word) { in_word = 1; words++; }
        }
        bytes += n;
    }
    if (name) printf("%ld %ld %ld %s\n", lines, words, bytes, name);
    else printf("%ld %ld %ld\n", lines, words, bytes);
}

int main(int argc, char** argv) {
    if (argc < 2) { count(0, 0); return 0; }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) { printf("wc: %s: no such file\n", argv[i]); return 1; }
        count(fd, argv[i]);
        close(fd);
    }
    return 0;
}
