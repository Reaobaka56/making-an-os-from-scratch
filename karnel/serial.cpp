// COM1 serial output. Use `qemu ... -serial stdio` to see it.
#include "serial.hpp"
#include "io.hpp"

static constexpr uint16_t COM1 = 0x3F8;

void serial_init() {
    outb(COM1 + 1, 0x00);  // no interrupts
    outb(COM1 + 3, 0x80);  // DLAB on
    outb(COM1 + 0, 0x03);  // 38400 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  // 8N1
    outb(COM1 + 2, 0xC7);  // FIFO
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, static_cast<uint8_t>(c));
}

int serial_getc_nb() { return (inb(COM1 + 5) & 1) ? inb(COM1) : -1; }
