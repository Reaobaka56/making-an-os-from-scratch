#pragma once
#include <stdint.h>

void console_init(void* fb, uint64_t w, uint64_t h, uint64_t pitch);
void kputc(char c);                 // framebuffer + serial
void kputs(const char* s);
void kprintf(const char* fmt, ...); // supports %s %c %d %u %x %lx %p %%
void console_clear();
