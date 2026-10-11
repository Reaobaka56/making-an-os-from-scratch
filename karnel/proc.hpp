#pragma once
#include <stdint.h>

struct ProcImage { uint64_t root, entry, rsp, brk; };

// Builds a fresh address space for the ELF at 'path' with argv on the stack. 0 on success, <0 on error.
int  proc_prepare(const char* path, int argc, char** argv, ProcImage* out);
// Starts it as a child of the calling thread. Returns pid or a negative error.
int  proc_spawn(const char* path, int argc = 0, char** argv = nullptr);
// Blocks until a child exits (pid < 0: any child). Returns the pid, or -1 if there is no such child.
int  proc_wait(int pid, int* status);
