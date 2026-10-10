#pragma once
#include <stdint.h>
bool     ata_init();                                         // primary master, PIO
uint32_t ata_sectors();
bool     ata_read(uint32_t lba, uint32_t count, void* buf);  // 512-byte sectors
bool     ata_write(uint32_t lba, uint32_t count, const void* buf);
