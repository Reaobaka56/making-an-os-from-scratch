// ATA PIO driver (primary channel, master drive, LBA28). Interrupts are disabled on the device; we poll.
#include "ata.hpp"
#include "io.hpp"
#include "irq.hpp"

namespace {
constexpr uint16_t DATA = 0x1F0, ERR = 0x1F1, COUNT = 0x1F2, LBA0 = 0x1F3, LBA1 = 0x1F4, LBA2 = 0x1F5,
                   DRIVE = 0x1F6, CMD = 0x1F7, CTRL = 0x3F6;
constexpr uint8_t BSY = 0x80, DRQ = 0x08, ERRB = 0x01, DF = 0x20;

uint32_t g_sectors;

void delay400() { for (int i = 0; i < 4; i++) inb(CTRL); }

bool wait_ready() {
    for (int i = 0; i < 1000000; i++) if (!(inb(CMD) & BSY)) return true;
    return false;
}

bool wait_drq() {
    delay400();
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(CMD);
        if (s & (ERRB | DF)) return false;
        if (!(s & BSY) && (s & DRQ)) return true;
    }
    return false;
}

void setup(uint32_t lba, uint8_t count, uint8_t cmd) {
    outb(DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(COUNT, count);
    outb(LBA0, lba & 0xFF);
    outb(LBA1, (lba >> 8) & 0xFF);
    outb(LBA2, (lba >> 16) & 0xFF);
    outb(CMD, cmd);
}
}  // namespace

bool ata_init() {
    outb(CTRL, 0x02);                  // nIEN: no interrupts
    outb(DRIVE, 0xA0);
    delay400();
    if (inb(CMD) == 0xFF) return false;  // floating bus: no drive
    outb(COUNT, 0); outb(LBA0, 0); outb(LBA1, 0); outb(LBA2, 0);
    outb(CMD, 0xEC);                   // IDENTIFY
    if (inb(CMD) == 0) return false;
    if (!wait_ready()) return false;
    if (inb(LBA1) || inb(LBA2)) return false;  // not ATA (ATAPI/SATA signature)
    if (!wait_drq()) return false;
    uint16_t id[256];
    insw(DATA, id, 256);
    g_sectors = id[60] | (uint32_t(id[61]) << 16);
    return g_sectors != 0;
}

uint32_t ata_sectors() { return g_sectors; }

bool ata_read(uint32_t lba, uint32_t count, void* buf) {
    auto* p = static_cast<uint8_t*>(buf);
    for (uint32_t i = 0; i < count; i++) {
        IrqGuard g;
        if (lba + i >= g_sectors || !wait_ready()) return false;
        setup(lba + i, 1, 0x20);
        if (!wait_drq()) return false;
        insw(DATA, p + i * 512, 256);
    }
    return true;
}

bool ata_write(uint32_t lba, uint32_t count, const void* buf) {
    auto* p = static_cast<const uint8_t*>(buf);
    for (uint32_t i = 0; i < count; i++) {
        IrqGuard g;
        if (lba + i >= g_sectors || !wait_ready()) return false;
        setup(lba + i, 1, 0x30);
        if (!wait_drq()) return false;
        outsw(DATA, p + i * 512, 256);
        outb(CMD, 0xE7);               // flush cache
        if (!wait_ready()) return false;
    }
    return true;
}
