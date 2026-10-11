// RaptorOS shell: pipelines (a | b | c), redirection (< file, > file), background jobs (&),
// built on fork + exec + wait + pipe + dup2.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMDS 8
#define MAX_ARGS 16
#define MAX_TOKS 64

typedef struct { char* argv[MAX_ARGS]; int argc; char* in; char* out; } Cmd;

static int readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        int c = getchar();
        if (c == EOF) { if (len == 0) return -1; continue; }   // ctrl-D on an empty line exits
        if (c == '\n') { putchar('\n'); buf[len] = 0; return len; }
        if (c == '\b' || c == 127) { if (len) { len--; fputs("\b \b", stdout); } continue; }
        if (c >= 32 && c < 127 && len < max - 1) { buf[len++] = c; putchar(c); }
    }
}

// Splits a line into words and the operators | < > & (which need no surrounding spaces).
static int tokenize(const char* line, char* store, size_t cap, char** toks) {
    int n = 0;
    size_t used = 0;
    for (const char* p = line; *p;) {
        if (*p == ' ') { p++; continue; }
        if (n >= MAX_TOKS - 1 || used + 2 >= cap) return -1;
        toks[n++] = store + used;
        if (strchr("|<>&", *p)) { store[used++] = *p++; }
        else while (*p && *p != ' ' && !strchr("|<>&", *p)) { if (used + 2 >= cap) return -1; store[used++] = *p++; }
        store[used++] = 0;
    }
    toks[n] = 0;
    return n;
}

static int parse(char** toks, int ntok, Cmd* cmds, int* background) {
    int n = 0;
    memset(&cmds[0], 0, sizeof cmds[0]);
    *background = 0;
    for (int i = 0; i < ntok; i++) {
        char* t = toks[i];
        if (!strcmp(t, "|")) {
            if (!cmds[n].argc || ++n >= MAX_CMDS) return -1;
            memset(&cmds[n], 0, sizeof cmds[n]);
        } else if (!strcmp(t, "<") || !strcmp(t, ">")) {
            if (i + 1 >= ntok || (toks[i + 1][1] == 0 && strchr("|<>&", toks[i + 1][0]))) return -1;   // needs a file name
            if (t[0] == '<') cmds[n].in = toks[++i]; else cmds[n].out = toks[++i];
        } else if (!strcmp(t, "&")) {
            if (i != ntok - 1) return -1;
            *background = 1;
        } else {
            if (cmds[n].argc >= MAX_ARGS - 1) return -1;
            cmds[n].argv[cmds[n].argc++] = t;
        }
    }
    if (!cmds[n].argc) return -1;
    return n + 1;
}

static void run_child(Cmd* c) {
    if (c->in) {
        int fd = open(c->in, O_RDONLY);
        if (fd < 0) { printf("%s: cannot open for reading\n", c->in); exit(1); }
        dup2(fd, 0); close(fd);
    }
    if (c->out) {
        int fd = open(c->out, O_WRONLY);
        if (fd < 0) { printf("%s: cannot open for writing\n", c->out); exit(1); }
        dup2(fd, 1); close(fd);
    }
    char path[80];
    if (strchr(c->argv[0], '/')) strncpy(path, c->argv[0], sizeof path - 1);
    else snprintf(path, sizeof path, "/bin/%s", c->argv[0]);
    c->argv[c->argc] = 0;
    execv(path, c->argv);
    printf("%s: command not found\n", c->argv[0]);
    exit(127);
}

static void run_line(Cmd* cmds, int n, int background) {
    int fds[MAX_CMDS][2];
    int pids[MAX_CMDS];
    for (int i = 0; i < n - 1; i++)
        if (pipe(fds[i]) < 0) { puts("pipe failed"); return; }

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] < 0) { puts("fork failed"); pids[i] = 0; break; }
        if (pids[i] == 0) {
            if (i > 0) dup2(fds[i - 1][0], 0);
            if (i < n - 1) dup2(fds[i][1], 1);
            for (int j = 0; j < n - 1; j++) { close(fds[j][0]); close(fds[j][1]); }
            run_child(&cmds[i]);
        }
    }
    for (int j = 0; j < n - 1; j++) { close(fds[j][0]); close(fds[j][1]); }   // parent keeps no pipe ends

    if (background) { printf("[%d] started\n", pids[n - 1]); return; }
    int status = 0;
    for (int i = 0; i < n; i++) {
        if (!pids[i]) continue;
        int st = 0;
        waitpid(pids[i], &st, 0);
        if (i == n - 1) status = st;                      // like a normal shell: the last stage decides
    }
    if (status != 0) printf("[exit %d]\n", status);
}

static void help(void) {
    puts("built-ins: help, exit\n"
         "programs in /bin: ls cat echo write rm ps mem uptime clear wc grep hello fault selftest\n"
         "pipes:      ls | grep bin | wc\n"
         "redirect:   echo hi > /f.txt     cat < /f.txt\n"
         "background: end a command with &");
}

int main(void) {
    puts("\nRaptorOS shell (user mode). Type 'help'.");
    char line[256], store[512];
    char* toks[MAX_TOKS];
    Cmd cmds[MAX_CMDS];
    for (;;) {
        fputs("os$ ", stdout);
        if (readline(line, sizeof line) < 0) break;
        int ntok = tokenize(line, store, sizeof store, toks);
        if (ntok < 0) { puts("line too long"); continue; }
        if (ntok == 0) continue;
        int background;
        int n = parse(toks, ntok, cmds, &background);
        if (n < 0) { puts("syntax error"); continue; }
        if (n == 1 && !cmds[0].in && !cmds[0].out) {
            if (!strcmp(cmds[0].argv[0], "exit")) break;
            if (!strcmp(cmds[0].argv[0], "help")) { help(); continue; }
        }
        run_line(cmds, n, background);
    }
    return 0;
}
