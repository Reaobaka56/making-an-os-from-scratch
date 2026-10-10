#pragma once
#include <stdint.h>

enum Sys : uint64_t {
    SYS_WRITE = 0,    // (fd, buf, len)  fd 1/2 = console
    SYS_READ = 1,     // (fd, buf, len)  fd 0 = keyboard/serial (blocks for 1 char), fd>=3 = file
    SYS_OPEN = 2,     // (path)          read-only, returns fd
    SYS_CLOSE = 3,    // (fd)
    SYS_EXIT = 4,     // (code)
    SYS_GETPID = 5,
    SYS_YIELD = 6,
    SYS_SLEEP = 7,    // (ms)
    SYS_UPTIME = 8,   // ms since boot
    SYS_SPAWN = 9,    // (path) -> pid
};

void syscall_init();
int  input_getc();   // blocking: keyboard or serial
