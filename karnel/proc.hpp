#pragma once
#include <stdint.h>
// Loads an ELF from the filesystem and starts it as a user process. Returns its pid, or a negative error.
int  proc_spawn(const char* path);
