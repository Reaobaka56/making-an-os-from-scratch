#include "console.hpp"
#include "font.hpp"
#include "serial.hpp"
#include <stdarg.h>
#include <stddef.h>

static uint8_t* g_fb; static uint64_t g_w, g_h, g_pitch;
static uint64_t g_col, g_row, g_cols, g_rows;
static uint32_t g_fg = 0xE0E0E0, g_bg = 0x101020;

static void draw_glyph(char c, uint64_t cx, uint64_t cy) {
    int idx = (c < FONT_FIRST || c > FONT_LAST) ? '?' - FONT_FIRST : c - FONT_FIRST;
    for (int y = 0; y < FONT_H; y++) {
        auto* row = reinterpret_cast<uint32_t*>(g_fb + (cy * FONT_H + y) * g_pitch) + cx * FONT_W;
        uint8_t bits = font_data[idx][y];
        for (int x = 0; x < FONT_W; x++) row[x] = (bits & (0x80 >> x)) ? g_fg : g_bg;
    }
}

static void scroll() {
    for (uint64_t y = 0; y < (g_rows - 1) * FONT_H; y++) {
        auto* dst = reinterpret_cast<uint64_t*>(g_fb + y * g_pitch);
        auto* src = reinterpret_cast<uint64_t*>(g_fb + (y + FONT_H) * g_pitch);
        for (uint64_t i = 0; i < g_cols * FONT_W / 2; i++) dst[i] = src[i];
    }
    for (uint64_t y = (g_rows - 1) * FONT_H; y < g_rows * FONT_H; y++) {
        auto* row = reinterpret_cast<uint32_t*>(g_fb + y * g_pitch);
        for (uint64_t x = 0; x < g_cols * FONT_W; x++) row[x] = g_bg;
    }
}

void console_init(void* fb, uint64_t w, uint64_t h, uint64_t pitch) {
    g_fb = static_cast<uint8_t*>(fb); g_w = w; g_h = h; g_pitch = pitch;
    g_cols = w / FONT_W; g_rows = h / FONT_H; g_col = g_row = 0;
    for (uint64_t y = 0; y < h; y++) {
        auto* row = reinterpret_cast<uint32_t*>(g_fb + y * pitch);
        for (uint64_t x = 0; x < w; x++) row[x] = g_bg;
    }
}

void kputc(char c) {
    serial_putc(c);
    if (!g_fb) return;
    if (c == '\n') { g_col = 0; g_row++; }
    else if (c == '\r') { g_col = 0; }
    else if (c == '\t') { g_col = (g_col + 4) & ~3ULL; }
    else { draw_glyph(c, g_col, g_row); g_col++; }
    if (g_col >= g_cols) { g_col = 0; g_row++; }
    if (g_row >= g_rows) { scroll(); g_row = g_rows - 1; }
}

void kputs(const char* s) { while (*s) kputc(*s++); }

static void put_num(uint64_t v, unsigned base, bool neg = false) {
    char buf[24]; int i = 0;
    do { buf[i++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    if (neg) kputc('-');
    while (i) kputc(buf[--i]);
}

void kprintf(const char* f, ...) {
    va_list ap; va_start(ap, f);
    for (; *f; f++) {
        if (*f != '%') { kputc(*f); continue; }
        f++;
        bool l = false;
        if (*f == 'l') { l = true; f++; }
        switch (*f) {
            case 's': { const char* s = va_arg(ap, const char*); kputs(s ? s : "(null)"); break; }
            case 'c': kputc(static_cast<char>(va_arg(ap, int))); break;
            case 'd': { int64_t v = l ? va_arg(ap, int64_t) : va_arg(ap, int);
                        if (v < 0) put_num(-static_cast<uint64_t>(v), 10, true); else put_num(v, 10); break; }
            case 'u': put_num(l ? va_arg(ap, uint64_t) : va_arg(ap, unsigned), 10); break;
            case 'x': put_num(l ? va_arg(ap, uint64_t) : va_arg(ap, unsigned), 16); break;
            case 'p': kputs("0x"); put_num(reinterpret_cast<uint64_t>(va_arg(ap, void*)), 16); break;
            case '%': kputc('%'); break;
            default: kputc('%'); kputc(*f); break;
        }
    }
    va_end(ap);
}
