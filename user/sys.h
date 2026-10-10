// Tiny userspace "libc": raw syscalls and printing helpers.
#pragma once
typedef long i64;
typedef unsigned long u64;

enum { SYS_WRITE, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_EXIT, SYS_GETPID, SYS_YIELD, SYS_SLEEP, SYS_UPTIME, SYS_SPAWN };

static inline i64 sys3(i64 n, i64 a, i64 b, i64 c) {
    i64 r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}
static inline i64 sys_write(int fd, const void* p, u64 n) { return sys3(SYS_WRITE, fd, (i64)p, n); }
static inline i64 sys_read(int fd, void* p, u64 n)  { return sys3(SYS_READ, fd, (i64)p, n); }
static inline i64 sys_open(const char* path)        { return sys3(SYS_OPEN, (i64)path, 0, 0); }
static inline i64 sys_close(int fd)                 { return sys3(SYS_CLOSE, fd, 0, 0); }
static inline __attribute__((noreturn)) void sys_exit(int code) { sys3(SYS_EXIT, code, 0, 0); for (;;) {} }
static inline i64 sys_getpid(void)                  { return sys3(SYS_GETPID, 0, 0, 0); }
static inline i64 sys_sleep(u64 ms)                 { return sys3(SYS_SLEEP, ms, 0, 0); }
static inline i64 sys_uptime(void)                  { return sys3(SYS_UPTIME, 0, 0, 0); }

static inline void print(const char* s) { u64 n = 0; while (s[n]) n++; sys_write(1, s, n); }
static inline void printnum(i64 v) {
    char b[24]; int i = 24; int neg = v < 0; u64 u = neg ? -(u64)v : (u64)v;
    do { b[--i] = '0' + u % 10; u /= 10; } while (u);
    if (neg) b[--i] = '-';
    sys_write(1, b + i, 24 - i);
}
