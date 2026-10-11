// malloc: first-fit free list on top of brk; big requests go straight to mmap.
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

typedef struct Block { size_t size; struct Block* next; int free; int mapped; } __attribute__((aligned(16))) Block;
#define HDR sizeof(Block)
#define BIG (256 * 1024)

static Block *head, *tail;

static int grow(size_t need) {
    size_t bytes = (need + HDR + 0xFFFF) & ~(size_t)0xFFFF;
    void* p = sbrk(bytes);
    if (p == (void*)-1) return 0;
    Block* b = p;
    b->size = bytes - HDR; b->next = 0; b->free = 1; b->mapped = 0;
    if (tail) tail->next = b; else head = b;
    tail = b;
    return 1;
}

static void coalesce(void) {
    for (Block* b = head; b && b->next;) {
        if (b->free && b->next->free && (char*)b + HDR + b->size == (char*)b->next) {
            if (b->next == tail) tail = b;
            b->size += HDR + b->next->size;
            b->next = b->next->next;
        } else b = b->next;
    }
}

void* malloc(size_t n) {
    n = n ? (n + 15) & ~(size_t)15 : 16;
    if (n >= BIG) {
        size_t total = (n + HDR + 4095) & ~(size_t)4095;
        Block* b = mmap(total, PROT_READ | PROT_WRITE);
        if (b == (void*)-1) return 0;
        b->size = total - HDR; b->next = 0; b->free = 0; b->mapped = 1;
        return (char*)b + HDR;
    }
    for (int attempt = 0; attempt < 2; attempt++) {
        for (Block* b = head; b; b = b->next) {
            if (!b->free || b->size < n) continue;
            if (b->size >= n + HDR + 16) {
                Block* r = (Block*)((char*)b + HDR + n);
                r->size = b->size - n - HDR; r->next = b->next; r->free = 1; r->mapped = 0;
                b->size = n; b->next = r;
                if (tail == b) tail = r;
            }
            b->free = 0;
            return (char*)b + HDR;
        }
        if (attempt == 0 && !grow(n)) return 0;
        if (attempt == 0) coalesce();
    }
    return 0;
}

void free(void* p) {
    if (!p) return;
    Block* b = (Block*)((char*)p - HDR);
    if (b->mapped) { munmap(b, b->size + HDR); return; }
    b->free = 1;
    coalesce();
}

void* calloc(size_t n, size_t sz) { void* p = malloc(n * sz); if (p) memset(p, 0, n * sz); return p; }

void* realloc(void* p, size_t n) {
    if (!p) return malloc(n);
    Block* b = (Block*)((char*)p - HDR);
    if (b->size >= n) return p;
    void* q = malloc(n);
    if (!q) return 0;
    memcpy(q, p, b->size);
    free(p);
    return q;
}

void exit(int code) { __sys(SYS_EXIT, code, 0, 0); for (;;) {} }
void abort(void) { exit(134); }
int abs(int v) { return v < 0 ? -v : v; }

long strtol(const char* s, char** end, int base) {
    while (isspace(*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    else if (base == 0) base = (s[0] == '0') ? 8 : 10;
    long v = 0;
    for (;; s++) {
        int d = isdigit(*s) ? *s - '0' : isalpha(*s) ? tolower(*s) - 'a' + 10 : 99;
        if (d >= base) break;
        v = v * base + d;
    }
    if (end) *end = (char*)s;
    return neg ? -v : v;
}
int atoi(const char* s) { return (int)strtol(s, 0, 10); }
