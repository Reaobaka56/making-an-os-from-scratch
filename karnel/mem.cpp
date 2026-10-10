// Freestanding builds still need these; the compiler may emit calls to them.
#include <stddef.h>
#include <stdint.h>
extern "C" {
void* memcpy(void* d, const void* s, size_t n) { auto* a = (uint8_t*)d; auto* b = (const uint8_t*)s; while (n--) *a++ = *b++; return d; }
void* memset(void* d, int c, size_t n) { auto* a = (uint8_t*)d; while (n--) *a++ = (uint8_t)c; return d; }
void* memmove(void* d, const void* s, size_t n) {
    auto* a = (uint8_t*)d; auto* b = (const uint8_t*)s;
    if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
int memcmp(const void* x, const void* y, size_t n) { auto* a = (const uint8_t*)x; auto* b = (const uint8_t*)y; while (n--) { if (*a != *b) return *a - *b; a++; b++; } return 0; }
}
