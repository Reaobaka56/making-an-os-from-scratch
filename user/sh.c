// RaptorOS shell: runs programs from /bin with fork + exec + wait.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        int c = getchar();
        if (c == EOF) return -1;
        if (c == '\n') { putchar('\n'); buf[len] = 0; return len; }
        if (c == '\b' || c == 127) { if (len) { len--; fputs("\b \b", stdout); } continue; }
        if (c == 4 && len == 0) return -1;                    // ctrl-D
        if (c >= 32 && c < 127 && len < max - 1) { buf[len++] = c; putchar(c); }
    }
}

static int split(char* line, char** argv, int max) {
    int n = 0;
    for (char* p = line; *p && n < max - 1;) {
        while (*p == ' ') *p++ = 0;
        if (!*p) break;
        argv[n++] = p;
        while (*p && *p != ' ') p++;
    }
    argv[n] = 0;
    return n;
}

static void help(void) {
    puts("built-ins: help, exit\n"
         "programs in /bin: ls cat echo write rm ps mem uptime clear hello fault selftest ...\n"
         "end a command with & to run it in the background");
}

int main(void) {
    puts("\nRaptorOS shell (user mode). Type 'help'.");
    char line[256], path[80];
    char* argv[16];
    for (;;) {
        fputs("os$ ", stdout);
        if (readline(line, sizeof line) < 0) break;
        int argc = split(line, argv, 16);
        if (!argc) continue;
        int background = 0;
        if (!strcmp(argv[argc - 1], "&")) { background = 1; argv[--argc] = 0; if (!argc) continue; }

        if (!strcmp(argv[0], "exit")) break;
        if (!strcmp(argv[0], "help")) { help(); continue; }

        if (strchr(argv[0], '/')) strncpy(path, argv[0], sizeof path - 1);
        else snprintf(path, sizeof path, "/bin/%s", argv[0]);

        int pid = fork();
        if (pid < 0) { puts("fork failed"); continue; }
        if (pid == 0) {
            execv(path, argv);
            printf("%s: command not found\n", argv[0]);
            exit(127);
        }
        if (background) { printf("[%d] started\n", pid); continue; }
        int status = 0;
        waitpid(pid, &status, 0);
        if (status != 0) printf("[exit %d]\n", status);
    }
    return 0;
}
