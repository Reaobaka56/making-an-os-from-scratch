#include <string.h>
#include <stdlib.h>

void* memcpy(void* d, const void* s, size_t n) { unsigned char* a = d; const unsigned char* b = s; while (n--) *a++ = *b++; return d; }
void* memmove(void* d, const void* s, size_t n) {
    unsigned char* a = d; const unsigned char* b = s;
    if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
void* memset(void* d, int c, size_t n) { unsigned char* a = d; while (n--) *a++ = (unsigned char)c; return d; }
int memcmp(const void* x, const void* y, size_t n) {
    const unsigned char *a = x, *b = y;
    while (n--) { if (*a != *b) return *a - *b; a++; b++; }
    return 0;
}
size_t strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char* a, const char* b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char* a, const char* b, size_t n) { while (n && *a && *a == *b) { a++; b++; n--; } return n ? (unsigned char)*a - (unsigned char)*b : 0; }
char* strcpy(char* d, const char* s) { char* r = d; while ((*d++ = *s++)) {} return r; }
char* strncpy(char* d, const char* s, size_t n) { size_t i = 0; for (; i < n && s[i]; i++) d[i] = s[i]; for (; i < n; i++) d[i] = 0; return d; }
char* strcat(char* d, const char* s) { strcpy(d + strlen(d), s); return d; }
char* strchr(const char* s, int c) { for (;; s++) { if (*s == (char)c) return (char*)s; if (!*s) return 0; } }
char* strrchr(const char* s, int c) { const char* r = 0; for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char*)r; } }
char* strstr(const char* h, const char* n) {
    size_t l = strlen(n);
    if (!l) return (char*)h;
    for (; *h; h++) if (!strncmp(h, n, l)) return (char*)h;
    return 0;
}
char* strdup(const char* s) { size_t n = strlen(s) + 1; char* p = malloc(n); if (p) memcpy(p, s, n); return p; }
