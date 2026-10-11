// RaptorOS system call interface (shared with the kernel's syscall.hpp numbers).
#pragma once
#include <stdint.h>
#include <stddef.h>

enum {
    SYS_WRITE, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_EXIT, SYS_GETPID, SYS_YIELD, SYS_SLEEP, SYS_UPTIME,
    SYS_SPAWN, SYS_FORK, SYS_EXEC, SYS_WAIT, SYS_BRK, SYS_MMAP, SYS_MUNMAP, SYS_UNLINK, SYS_READDIR,
    SYS_PS, SYS_MEMINFO, SYS_DUP, SYS_DUP2, SYS_PIPE
};

typedef struct { char name[48]; uint32_t size; } DirInfo;
typedef struct { uint64_t free_frames, total_frames, heap_kib, disk_free_kib, uptime_ms, threads; } MemInfo;
typedef struct { uint64_t pid, parent; char state, user; char name[24]; } PsInfo;

static inline long __sys(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}
