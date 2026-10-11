#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static FILE f_in = {.fd = 0}, f_out = {.fd = 1}, f_err = {.fd = 2};
FILE *stdin = &f_in, *stdout = &f_out, *stderr = &f_err;

typedef void (*putfn)(void* ctx, char c);

static int fmt_num(char* out, unsigned long v, unsigned base, int neg, int upper) {
    char tmp[24]; int n = 0, len = 0;
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[n++] = digits[v % base]; v /= base; } while (v);
    if (neg) out[len++] = '-';
    while (n) out[len++] = tmp[--n];
    return len;
}

static int emit(putfn put, void* ctx, const char* s, int len, int width, int left, char pad) {
    int count = 0;
    if (!left) for (int i = len; i < width; i++) { put(ctx, pad); count++; }
    for (int i = 0; i < len; i++) { put(ctx, s[i]); count++; }
    if (left) for (int i = len; i < width; i++) { put(ctx, ' '); count++; }
    return count;
}

static int vformat(putfn put, void* ctx, const char* f, va_list ap) {
    int count = 0;
    for (; *f; f++) {
        if (*f != '%') { put(ctx, *f); count++; continue; }
        f++;
        int left = 0, width = 0, lng = 0; char pad = ' ';
        if (*f == '-') { left = 1; f++; }
        if (*f == '0' && !left) { pad = '0'; f++; }
        if (*f == '*') { width = va_arg(ap, int); f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        while (*f == 'l' || *f == 'z') { lng = 1; f++; }
        char tmp[32];
        switch (*f) {
        case 's': { const char* s = va_arg(ap, const char*); if (!s) s = "(null)"; count += emit(put, ctx, s, strlen(s), width, left, ' '); break; }
        case 'c': { char c = (char)va_arg(ap, int); count += emit(put, ctx, &c, 1, width, left, ' '); break; }
        case 'd': case 'i': {
            long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            int n = v < 0 ? fmt_num(tmp, -(unsigned long)v, 10, 1, 0) : fmt_num(tmp, v, 10, 0, 0);
            count += emit(put, ctx, tmp, n, width, left, pad); break; }
        case 'u': { unsigned long v = lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            int n = fmt_num(tmp, v, 10, 0, 0); count += emit(put, ctx, tmp, n, width, left, pad); break; }
        case 'x': case 'X': { unsigned long v = lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            int n = fmt_num(tmp, v, 16, 0, *f == 'X'); count += emit(put, ctx, tmp, n, width, left, pad); break; }
        case 'p': { put(ctx, '0'); put(ctx, 'x'); count += 2;
            int n = fmt_num(tmp, (unsigned long)va_arg(ap, void*), 16, 0, 0); count += emit(put, ctx, tmp, n, width, left, pad); break; }
        case '%': put(ctx, '%'); count++; break;
        case 0: return count;
        default: put(ctx, '%'); put(ctx, *f); count += 2; break;
        }
    }
    return count;
}

typedef struct { int fd; int n; char buf[256]; } Out;
static void out_flush(Out* o) { if (o->n) { write(o->fd, o->buf, o->n); o->n = 0; } }
static void out_put(void* ctx, char c) { Out* o = ctx; if (o->n == (int)sizeof o->buf) out_flush(o); o->buf[o->n++] = c; }

int vfprintf(FILE* f, const char* fmt, va_list ap) {
    Out o = {f->fd, 0, {0}};
    int n = vformat(out_put, &o, fmt, ap);
    out_flush(&o);
    return n;
}
int printf(const char* fmt, ...) { va_list ap; va_start(ap, fmt); int n = vfprintf(stdout, fmt, ap); va_end(ap); return n; }
int fprintf(FILE* f, const char* fmt, ...) { va_list ap; va_start(ap, fmt); int n = vfprintf(f, fmt, ap); va_end(ap); return n; }

typedef struct { char* buf; size_t cap, n; } SOut;
static void sout_put(void* ctx, char c) { SOut* s = ctx; if (s->n + 1 < s->cap) s->buf[s->n] = c; s->n++; }
int vsnprintf(char* buf, size_t n, const char* fmt, va_list ap) {
    SOut s = {buf, n, 0};
    int r = vformat(sout_put, &s, fmt, ap);
    if (n) buf[s.n < n ? s.n : n - 1] = 0;
    return r;
}
int snprintf(char* buf, size_t n, const char* fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, n, fmt, ap); va_end(ap); return r; }
int sprintf(char* buf, const char* fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, 1 << 20, fmt, ap); va_end(ap); return r; }

int fputs(const char* s, FILE* f) { return write(f->fd, s, strlen(s)) < 0 ? EOF : 0; }
int puts(const char* s) { fputs(s, stdout); return write(1, "\n", 1) < 0 ? EOF : 0; }
int fputc(int c, FILE* f) { char ch = c; return write(f->fd, &ch, 1) < 0 ? EOF : c; }
int putchar(int c) { return fputc(c, stdout); }

int fgetc(FILE* f) {
    if (f->rpos >= f->rlen) {
        long n = read(f->fd, f->rbuf, sizeof f->rbuf);
        if (n <= 0) { f->eof = 1; return EOF; }
        f->rlen = n; f->rpos = 0;
    }
    return f->rbuf[f->rpos++];
}
int getchar(void) { return fgetc(stdin); }

char* fgets(char* buf, int n, FILE* f) {
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    buf[i] = 0;
    return buf;
}

size_t fread(void* buf, size_t sz, size_t n, FILE* f) {
    size_t total = sz * n, got = 0;
    unsigned char* p = buf;
    while (got < total) { int c = fgetc(f); if (c == EOF) break; p[got++] = c; }
    return sz ? got / sz : 0;
}
size_t fwrite(const void* buf, size_t sz, size_t n, FILE* f) {
    long r = write(f->fd, buf, sz * n);
    return r < 0 ? 0 : (sz ? r / sz : 0);
}

FILE* fopen(const char* path, const char* mode) {
    int fd = open(path, mode[0] == 'w' ? O_WRONLY : O_RDONLY);
    if (fd < 0) return 0;
    FILE* f = calloc(1, sizeof *f);
    if (!f) { close(fd); return 0; }
    f->fd = fd;
    return f;
}
int fclose(FILE* f) { int r = close(f->fd); free(f); return r; }
