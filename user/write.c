#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char** argv) {
    if (argc < 3) { fputs("usage: write <file> <text...>\n", stderr); return 1; }
    FILE* f = fopen(argv[1], "w");
    if (!f) { puts("write: cannot create file"); return 1; }
    for (int i = 2; i < argc; i++) { if (i > 2) fputc(' ', f); fputs(argv[i], f); }
    fputc('\n', f);
    if (fclose(f) != 0) { puts("write: could not save file (disk full?)"); return 1; }
    return 0;
}
