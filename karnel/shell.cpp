// Tiny interactive shell. Input comes from the PS/2 keyboard or the serial port.
#include "shell.hpp"
#include "syscall.hpp"
#include "sched.hpp"
#include "fs.hpp"
#include "proc.hpp"
#include "pmm.hpp"
#include "heap.hpp"
#include "apic.hpp"
#include "console.hpp"
#include <string.h>

namespace {
int readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        int c = input_getc();
        if (c == '\n') { kputc('\n'); buf[len] = 0; return len; }
        if (c == '\b' || c == 127) { if (len) { len--; kputs("\b \b"); } continue; }
        if (c >= 32 && c < 127 && len < max - 1) { buf[len++] = c; kputc(c); }
    }
}

int split(char* line, char** argv, int max) {
    int n = 0;
    for (char* p = line; *p && n < max;) {
        while (*p == ' ') *p++ = 0;
        if (!*p) break;
        argv[n++] = p;
        while (*p && *p != ' ') p++;
    }
    return n;
}

void run_program(const char* path) {
    int pid = proc_spawn(path);
    if (pid < 0) { kprintf("run: cannot start %s (error %d)\n", path, pid); return; }
    int status = 0;
    proc_wait(pid, &status);
    kprintf("[pid %d exited with code %d]\n", pid, status);
}

void cmd_cat(const char* path) {
    int idx = fs_find(path);
    if (idx < 0) { kprintf("cat: %s: no such file\n", path); return; }
    char buf[256];
    uint32_t off = 0;
    for (;;) {
        int64_t n = fs_read(idx, off, buf, sizeof buf);
        if (n <= 0) break;
        for (int64_t i = 0; i < n; i++) kputc(buf[i]);
        off += n;
    }
    if (off && buf[(off - 1) % sizeof buf] != '\n') kputc('\n');
}

void help() {
    kputs("commands:\n"
          "  help              this text\n"
          "  ls                list files\n"
          "  cat <file>        print a file\n"
          "  write <f> <text>  create/overwrite a file\n"
          "  rm <file>         delete a file\n"
          "  run <file>        run a user program (ELF)\n"
          "  ps                threads and processes\n"
          "  mem               memory usage\n"
          "  df                disk usage\n"
          "  uptime            time since boot\n"
          "  echo <text>       print text\n"
          "  clear             clear the screen\n");
}
}  // namespace

void shell_main(void*) {
    kputs("\nRaptorOS shell. Type 'help'.\n");
    char line[128];
    char* argv[16];
    for (;;) {
        kputs("os> ");
        readline(line, sizeof line);
        char raw[128];
        memcpy(raw, line, sizeof raw); raw[sizeof raw - 1] = 0;
        int argc = split(line, argv, 16);
        if (!argc) continue;
        const char* cmd = argv[0];

        if (!strcmp(cmd, "help")) help();
        else if (!strcmp(cmd, "ls")) {
            for (int i = 0; i < fs_capacity(); i++)
                if (fs_used(i)) kprintf("  %-24s %6u bytes\n", fs_name(i), fs_size(i));
        }
        else if (!strcmp(cmd, "cat") && argc > 1) cmd_cat(argv[1]);
        else if (!strcmp(cmd, "write") && argc > 2) {
            const char* text = raw + (argv[2] - line);       // everything after the file name
            if (fs_write(argv[1], text, strlen(text))) kprintf("wrote %u bytes to %s\n", (unsigned)strlen(text), argv[1]);
            else kputs("write failed\n");
        }
        else if (!strcmp(cmd, "rm") && argc > 1) kputs(fs_remove(argv[1]) ? "removed\n" : "rm: no such file\n");
        else if (!strcmp(cmd, "run") && argc > 1) run_program(argv[1]);
        else if (!strcmp(cmd, "ps")) sched_dump();
        else if (!strcmp(cmd, "mem"))
            kprintf("  frames: %lu free of %lu (%lu MiB free)\n  heap mapped: %lu KiB\n",
                    pmm_free_frames(), pmm_total_frames(), pmm_free_frames() * 4 / 1024, heap_bytes_mapped() / 1024);
        else if (!strcmp(cmd, "df")) kprintf("  free: %u KiB\n", fs_free_bytes() / 1024);
        else if (!strcmp(cmd, "uptime")) kprintf("  %lu ms (%lu ticks)\n", uptime_ms(), timer_ticks());
        else if (!strcmp(cmd, "echo")) { if (argc > 1) kputs(raw + (argv[1] - line)); kputc('\n'); }
        else if (!strcmp(cmd, "clear")) console_clear();
        else kprintf("unknown command: %s (try 'help')\n", cmd);
    }
}
