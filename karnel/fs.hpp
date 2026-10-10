#pragma once
#include <stdint.h>
#include <stddef.h>

// RaptorFS: flat namespace of named files (names may contain '/'), contiguous extents.
bool     fs_mount();
bool     fs_mounted();
int      fs_find(const char* name);                       // index or -1
uint32_t fs_size(int idx);
const char* fs_name(int idx);
int      fs_capacity();
bool     fs_used(int idx);
int64_t  fs_read(int idx, uint32_t off, void* buf, uint32_t n);   // bytes read
bool     fs_write(const char* name, const void* data, uint32_t len);  // create or replace
bool     fs_remove(const char* name);
uint32_t fs_free_bytes();
