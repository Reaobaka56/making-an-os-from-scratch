#pragma once
#include <stdint.h>
#include "sched.hpp"

// Per-process file descriptor table: console, files, write-mode files and pipes, all reference counted
// so fork/dup/dup2 can share them.
void    fd_init_std(Thread* t);                                  // fd 0 = console in, 1 and 2 = console out
int     fd_alloc(Thread* t);                                     // lowest free descriptor or -1
void    fd_dup_into(OpenFile& dst, const OpenFile& src);        // copies src into dst, taking a reference
bool    fd_release(OpenFile& f, bool can_block);                 // drops the reference; false if a commit failed
void    fd_close_all(Thread* t, bool can_block);

Pipe*   pipe_create();                                           // 1 reader + 1 writer reference
int64_t pipe_read(Pipe* p, void* buf, uint64_t n);               // blocks; 0 = EOF
int64_t pipe_write(Pipe* p, const void* buf, uint64_t n);        // blocks; -1 if no readers

WFile*  wfile_create(const char* name);
int64_t wfile_write(WFile* f, const void* buf, uint64_t n);
