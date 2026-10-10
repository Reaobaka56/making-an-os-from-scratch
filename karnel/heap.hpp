#pragma once
#include <stddef.h>
#include <stdint.h>

void  heap_init();
void* kmalloc(size_t n);
void  kfree(void* p);
size_t heap_free_blocks();   // for tests: 1 means fully coalesced
size_t heap_bytes_mapped();
