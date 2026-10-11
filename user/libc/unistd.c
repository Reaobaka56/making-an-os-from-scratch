#include <unistd.h>

long read(int fd, void* buf, size_t n)        { return __sys(SYS_READ, fd, (long)buf, n); }
long write(int fd, const void* buf, size_t n) { return __sys(SYS_WRITE, fd, (long)buf, n); }
int  open(const char* path, int flags)        { return __sys(SYS_OPEN, (long)path, flags, 0); }
int  close(int fd)                            { return __sys(SYS_CLOSE, fd, 0, 0); }
int  unlink(const char* path)                 { return __sys(SYS_UNLINK, (long)path, 0, 0); }
int  fork(void)                               { return __sys(SYS_FORK, 0, 0, 0); }
int  execv(const char* path, char* const argv[]) { return __sys(SYS_EXEC, (long)path, (long)argv, 0); }
int  waitpid(int pid, int* status, int options) { (void)options; return __sys(SYS_WAIT, pid, (long)status, 0); }
int  wait(int* status)                        { return waitpid(-1, status, 0); }
int  spawn(const char* path, char* const argv[]) { return __sys(SYS_SPAWN, (long)path, (long)argv, 0); }
int  getpid(void)                             { return __sys(SYS_GETPID, 0, 0, 0); }
void yield(void)                              { __sys(SYS_YIELD, 0, 0, 0); }
void msleep(unsigned long ms)                 { __sys(SYS_SLEEP, ms, 0, 0); }
unsigned long uptime_ms(void)                 { return __sys(SYS_UPTIME, 0, 0, 0); }
int  brk(void* addr)                          { long r = __sys(SYS_BRK, (long)addr, 0, 0); return r == (long)addr ? 0 : -1; }
void* sbrk(long incr) {
    long cur = __sys(SYS_BRK, 0, 0, 0);
    if (incr == 0) return (void*)cur;
    if (__sys(SYS_BRK, cur + incr, 0, 0) != cur + incr) return (void*)-1;
    return (void*)cur;
}
void* mmap(size_t len, int prot) { long r = __sys(SYS_MMAP, len, prot, 0); return r < 0 ? (void*)-1 : (void*)r; }
int   munmap(void* addr, size_t len)          { return __sys(SYS_MUNMAP, (long)addr, len, 0); }
int   readdir(int slot, DirInfo* out)         { return __sys(SYS_READDIR, slot, (long)out, 0); }
int   ps(PsInfo* out, int max)                { return __sys(SYS_PS, (long)out, max, 0); }
int   meminfo(MemInfo* out)                   { return __sys(SYS_MEMINFO, (long)out, 0, 0); }
