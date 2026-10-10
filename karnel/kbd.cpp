// PS/2 keyboard, scancode set 1, US layout.
#include "kbd.hpp"
#include "apic.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "irq.hpp"

namespace {
const char normal[58] = {0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
const char shifted[58] = {0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '};

constexpr int N = 256;
char buf[N];
volatile int head, tail;
bool shift, ctrl, caps, ext;

void push(char c) { int n = (head + 1) % N; if (n != tail) { buf[head] = c; head = n; } }

void irq(Frame*) {
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { ext = true; return; }
    bool release = sc & 0x80; sc &= 0x7F;
    if (ext) { ext = false; return; }            // arrows etc. ignored for now
    if (sc == 0x2A || sc == 0x36) { shift = !release; return; }
    if (sc == 0x1D) { ctrl = !release; return; }
    if (sc == 0x3A) { if (!release) caps = !caps; return; }
    if (release || sc >= 58) return;
    char c = shift ? shifted[sc] : normal[sc];
    if (caps) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
    }
    if (ctrl && c >= 'a' && c <= 'z') c -= 96;   // ctrl-a = 1 ...
    if (c) push(c);
}
}  // namespace

void kbd_init() {
    idt_set_handler(VEC_KBD, irq);
    ioapic_route(1, VEC_KBD);
    while (inb(0x64) & 1) inb(0x60);             // drain
}

int kbd_pop() {
    IrqGuard g;
    if (head == tail) return -1;
    char c = buf[tail]; tail = (tail + 1) % N;
    return static_cast<unsigned char>(c);
}
