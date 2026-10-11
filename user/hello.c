#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("Hello from userspace! pid=%d, %d argument(s):", getpid(), argc);
    for (int i = 0; i < argc; i++) printf(" [%s]", argv[i]);
    printf("\n");

    FILE* f = fopen("/etc/motd", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof line, f)) fputs(line, stdout);
        fclose(f);
    } else {
        puts("motd missing");
    }

    long bad = write(1, (const char*)0x1234, 5);        // the kernel must reject this, not crash
    printf("write to a bad pointer returned %ld\n", bad);

    char* p = malloc(100);
    snprintf(p, 100, "malloc works (%d bytes at %p)", 100, (void*)p);
    puts(p);
    free(p);
    return 42;
}
