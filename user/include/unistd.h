#pragma once
#include "os.h"

#define O_RDONLY 0
#define O_WRONLY 1
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

long read(int fd, void* buf, size_t n);
long write(int fd, const void* buf, size_t n);
int  open(const char* path, int flags);
int  close(int fd);
int  unlink(const char* path);
int  dup(int fd);
int  dup2(int oldfd, int newfd);
int  pipe(int fds[2]);        // fds[0] = read end, fds[1] = write end
int  fork(void);
int  execv(const char* path, char* const argv[]);
int  wait(int* status);
int  waitpid(int pid, int* status, int options);
int  spawn(const char* path, char* const argv[]);
int  getpid(void);
void yield(void);
void msleep(unsigned long ms);
unsigned long uptime_ms(void);
void* sbrk(long incr);
int   brk(void* addr);
void* mmap(size_t len, int prot);
int   munmap(void* addr, size_t len);
int   readdir(int slot, DirInfo* out);
int   ps(PsInfo* out, int max);
int   meminfo(MemInfo* out);
