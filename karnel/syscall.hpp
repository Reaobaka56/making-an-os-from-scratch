#pragma once
#include <stdint.h>

enum Sys : uint64_t {
    SYS_WRITE = 0,    // (fd, buf, len)   fd 1/2 = console, fd>=3 = file opened for writing
    SYS_READ = 1,     // (fd, buf, len)   fd 0 = keyboard/serial (blocks for 1 char), fd>=3 = file
    SYS_OPEN = 2,     // (path, flags)    flags: 0 = read, 1 = write (create/truncate, committed on close)
    SYS_CLOSE = 3,    // (fd)
    SYS_EXIT = 4,     // (code)
    SYS_GETPID = 5,
    SYS_YIELD = 6,
    SYS_SLEEP = 7,    // (ms)
    SYS_UPTIME = 8,   // ms since boot
    SYS_SPAWN = 9,    // (path, argv) -> pid   (fork+exec in one step)
    SYS_FORK = 10,    // () -> child pid in the parent, 0 in the child
    SYS_EXEC = 11,    // (path, argv) -> only returns on error
    SYS_WAIT = 12,    // (pid or -1, int* status) -> pid
    SYS_BRK = 13,     // (addr or 0) -> new program break (current break on failure)
    SYS_MMAP = 14,    // (len, prot) anonymous private mapping; prot bit0=R bit1=W bit2=X
    SYS_MUNMAP = 15,  // (addr, len)
    SYS_UNLINK = 16,  // (path)
    SYS_READDIR = 17, // (slot, DirInfo*) -> 0 used, 1 empty slot, -1 past the end
    SYS_PS = 18,      // (PsInfo*, max) -> count
    SYS_MEMINFO = 19, // (MemInfo*)
};

struct DirInfo { char name[48]; uint32_t size; };
struct MemInfo { uint64_t free_frames, total_frames, heap_kib, disk_free_kib, uptime_ms, threads; };

void syscall_init();
int  input_getc();   // blocking: keyboard or serial
