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
size_t strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char* a, const char* b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char* a, const char* b, size_t n) { while (n && *a && *a == *b) { a++; b++; n--; } return n ? (unsigned char)*a - (unsigned char)*b : 0; }
char* strncpy(char* d, const char* s, size_t n) { size_t i = 0; for (; i < n && s[i]; i++) d[i] = s[i]; for (; i < n; i++) d[i] = 0; return d; }
}
