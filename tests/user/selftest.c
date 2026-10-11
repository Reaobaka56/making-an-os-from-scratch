// User-space test suite: exercises libc, brk/mmap, fork/exec/wait, argv, pipes, dup2 and the filesystem.
// Prints one PASS/FAIL line per check and "SELFTEST DONE failures=N"; exits with the failure count.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
static int global_counter = 1;

static void check(const char* name, int ok) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

static void test_argv(int argc, char** argv) {
    check("argc >= 1", argc >= 1);
    check("argv[0] is the program name", argv[0] && strlen(argv[0]) > 0);
    check("argv[argc] is NULL", argv[argc] == 0);
}

static void test_libc(void) {
    char buf[64];
    snprintf(buf, sizeof buf, "%d|%5d|%-5d|%05d|%x|%s|%c", -42, 7, 7, 42, 255, "str", 'z');
    check("snprintf formats", strcmp(buf, "-42|    7|7    |00042|ff|str|z") == 0);
    check("strtol parses hex and negatives", strtol("-0x1f", 0, 0) == -31 && atoi("1234") == 1234);
    check("strstr/strchr/strrchr", strstr("hello world", "o w") && *strchr("abc", 'b') == 'b' && *strrchr("a/b/c", '/') == '/');
    check("memmove handles overlap", (memcpy(buf, "abcdef", 7), memmove(buf + 2, buf, 4), memcmp(buf, "ababcd", 6) == 0));
}

static void test_malloc(void) {
    void* ptrs[200];
    int ok = 1;
    for (int i = 0; i < 200; i++) {
        ptrs[i] = malloc(16 + i * 7);
        if (!ptrs[i]) { ok = 0; break; }
        memset(ptrs[i], i, 16 + i * 7);
    }
    for (int i = 0; ok && i < 200; i++) ok = ((unsigned char*)ptrs[i])[15 + i * 7] == (unsigned char)i;
    check("200 mallocs hold their contents", ok);
    for (int i = 0; i < 200; i++) free(ptrs[i]);

    char* big = malloc(1 << 20);                         // above the mmap threshold
    check("1 MiB malloc (mmap path)", big != 0);
    if (big) { big[0] = 1; big[(1 << 20) - 1] = 2; check("big block readable", big[0] == 1 && big[(1 << 20) - 1] == 2); free(big); }

    char* r = malloc(10);
    strcpy(r, "grow me");
    r = realloc(r, 5000);
    check("realloc preserves data", r && strcmp(r, "grow me") == 0);
    free(r);
    char* z = calloc(100, 4);
    int zero = 1;
    for (int i = 0; i < 400; i++) zero &= z[i] == 0;
    check("calloc zeroes", zero);
    free(z);
}

static void test_brk_mmap(void) {
    char* start = sbrk(0);
    char* p = sbrk(3 * 4096);
    check("sbrk grows the heap", p == start && p != (char*)-1);
    p[0] = 'a'; p[3 * 4096 - 1] = 'b';
    check("new brk memory is usable", p[0] == 'a' && p[3 * 4096 - 1] == 'b');
    sbrk(-3 * 4096);
    check("sbrk shrinks back", sbrk(0) == start);

    char* m = mmap(5 * 4096, PROT_READ | PROT_WRITE);
    check("mmap returns memory", m != (char*)-1);
    if (m != (char*)-1) {
        int zero = 1;
        for (int i = 0; i < 5 * 4096; i += 512) zero &= m[i] == 0;
        m[0] = 7; m[5 * 4096 - 1] = 9;
        check("mmap memory is zeroed and writable", zero && m[0] == 7 && m[5 * 4096 - 1] == 9);
        check("munmap succeeds", munmap(m, 5 * 4096) == 0);
    }
    check("mmap rejects absurd sizes", mmap((size_t)1 << 40, PROT_READ) == (void*)-1);
}

static void test_fork_wait(void) {
    int status = -1;
    int pid = fork();
    if (pid == 0) exit(7);
    check("fork returns a child pid", pid > 0);
    check("waitpid returns the child's pid and exit code", waitpid(pid, &status, 0) == pid && status == 7);

    // Address spaces are separate after fork.
    char* heap = malloc(16);
    strcpy(heap, "parent");
    global_counter = 1;
    pid = fork();
    if (pid == 0) { global_counter = 99; strcpy(heap, "child"); exit(global_counter == 99 && !strcmp(heap, "child") ? 0 : 1); }
    waitpid(pid, &status, 0);
    check("child ran with its own copy", status == 0);
    check("parent memory unchanged by child", global_counter == 1 && !strcmp(heap, "parent"));
    free(heap);

    // wait(-1) reaps several children.
    int sum = 0;
    for (int i = 1; i <= 3; i++) if (fork() == 0) exit(i);
    for (int i = 0; i < 3; i++) { if (wait(&status) > 0) sum += status; }
    check("wait(-1) collects three children", sum == 6);
    check("wait with no children fails", wait(&status) == -1);
}

static void test_exec(void) {
    int status = -1;
    int pid = fork();
    if (pid == 0) {
        char* args[] = {"echo", "exec", "works", 0};
        execv("/bin/echo", args);
        exit(99);
    }
    waitpid(pid, &status, 0);
    check("fork+exec runs /bin/echo", status == 0);

    pid = fork();
    if (pid == 0) { char* args[] = {"nope", 0}; execv("/bin/does-not-exist", args); exit(55); }
    waitpid(pid, &status, 0);
    check("exec of a missing file returns an error", status == 55);

    char* args[] = {"echo", "spawned", 0};
    pid = spawn("/bin/echo", args);
    check("spawn starts a program", pid > 0 && waitpid(pid, &status, 0) == pid && status == 0);
}

static void test_files(void) {
    const char* name = "/selftest.tmp";
    FILE* f = fopen(name, "w");
    check("create a file", f != 0);
    if (f) {
        for (int i = 0; i < 100; i++) fprintf(f, "line %d\n", i);     // spans several sectors
        check("close commits the file", fclose(f) == 0);
    }
    f = fopen(name, "r");
    check("reopen for reading", f != 0);
    if (f) {
        char line[64]; int n = 0, ok = 1;
        while (fgets(line, sizeof line, f)) { char want[32]; snprintf(want, sizeof want, "line %d\n", n); ok &= !strcmp(line, want); n++; }
        check("read back 100 lines intact", n == 100 && ok);
        fclose(f);
    }
    int found = 0;
    DirInfo d;
    for (int s = 0; readdir(s, &d) >= 0; s++) if (!strcmp(d.name, name)) found = 1;
    check("readdir lists the new file", found);
    check("unlink removes it", unlink(name) == 0 && open(name, O_RDONLY) < 0);
    check("unlink of a missing file fails", unlink(name) < 0);
    check("open of a missing file fails", open("/nope", O_RDONLY) < 0);
}

static void test_pipes(void) {
    int fds[2];
    check("pipe() creates a pair", pipe(fds) == 0 && fds[0] >= 3 && fds[1] > fds[0]);
    check("write to the pipe", write(fds[1], "ping", 4) == 4);
    char buf[16] = {0};
    check("read it back from the other end", read(fds[0], buf, sizeof buf) == 4 && !strcmp(buf, "ping"));
    close(fds[1]);
    check("EOF once the writer is closed", read(fds[0], buf, sizeof buf) == 0);
    close(fds[0]);

    pipe(fds);
    close(fds[0]);
    check("write with no readers fails", write(fds[1], "x", 1) < 0);
    close(fds[1]);

    // Parent -> child through a pipe, more data than the 4 KiB pipe buffer holds (forces blocking).
    pipe(fds);
    int pid = fork();
    if (pid == 0) {
        close(fds[1]);
        long sum = 0, total = 0; unsigned char chunk[300]; long n;
        while ((n = read(fds[0], chunk, sizeof chunk)) > 0) { for (long i = 0; i < n; i++) sum += chunk[i]; total += n; }
        exit(total == 20000 && sum == 20000L * 7 ? 0 : 1);
    }
    close(fds[0]);
    unsigned char block[500];
    memset(block, 7, sizeof block);
    int ok = 1;
    for (int i = 0; i < 40; i++) ok &= write(fds[1], block, sizeof block) == (long)sizeof block;
    close(fds[1]);
    int status = -1;
    waitpid(pid, &status, 0);
    check("20000 bytes through a 4 KiB pipe, intact", ok && status == 0);

    // dup2 redirects stdout into a pipe: a child's printf lands in the parent's read end.
    pipe(fds);
    pid = fork();
    if (pid == 0) { dup2(fds[1], 1); close(fds[0]); close(fds[1]); printf("redirected\n"); exit(0); }
    close(fds[1]);
    char out[32] = {0};
    long got = read(fds[0], out, sizeof out - 1);
    close(fds[0]);
    waitpid(pid, &status, 0);
    check("dup2 redirects a child's stdout into a pipe", got == 11 && !strcmp(out, "redirected\n"));

    int copy = dup(1);
    check("dup returns a new descriptor", copy > 2 && copy != 1);
    close(copy);
    check("dup2 on a bad descriptor fails", dup2(15, 3) < 0);
}

static void test_redirect_file(void) {
    int fd = open("/redir.tmp", O_WRONLY);
    int pid = fork();
    if (pid == 0) { dup2(fd, 1); close(fd); printf("into a file\n"); exit(0); }
    close(fd);                                          // the child holds the last reference
    int status;
    waitpid(pid, &status, 0);
    FILE* f = fopen("/redir.tmp", "r");
    char line[64] = {0};
    if (f) { fgets(line, sizeof line, f); fclose(f); }
    check("stdout redirected to a file is committed on exit", f && !strcmp(line, "into a file\n"));
    unlink("/redir.tmp");
}

static void test_misc(void) {
    unsigned long t0 = uptime_ms();
    msleep(50);
    unsigned long dt = uptime_ms() - t0;
    check("msleep(50) sleeps about 50 ms", dt >= 40 && dt < 400);
    PsInfo p[32];
    int n = ps(p, 32), me = 0;
    for (int i = 0; i < n; i++) me |= (int)p[i].pid == getpid();
    check("ps lists this process", me);
    MemInfo m;
    check("meminfo works", meminfo(&m) == 0 && m.total_frames > 0);
}

int main(int argc, char** argv) {
    puts("selftest:");
    test_argv(argc, argv);
    test_libc();
    test_malloc();
    test_brk_mmap();
    test_fork_wait();
    test_exec();
    test_files();
    test_pipes();
    test_redirect_file();
    test_misc();
    printf("SELFTEST DONE failures=%d\n", failures);
    return failures;
}
