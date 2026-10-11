// grep PATTERN [file...]: prints lines containing PATTERN (plain substring). Exit 0 if anything matched.
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int scan(FILE* f, const char* pat) {
    char line[512];
    int found = 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, pat)) { fputs(line, stdout); size_t n = strlen(line); if (n && line[n - 1] != '\n') putchar('\n'); found = 1; }
    }
    return found;
}

int main(int argc, char** argv) {
    if (argc < 2) { fputs("usage: grep PATTERN [file...]\n", stderr); return 2; }
    int found = 0;
    if (argc == 2) found = scan(stdin, argv[1]);
    for (int i = 2; i < argc; i++) {
        FILE* f = fopen(argv[i], "r");
        if (!f) { printf("grep: %s: no such file\n", argv[i]); continue; }
        found |= scan(f, argv[1]);
        fclose(f);
    }
    return found ? 0 : 1;
}
