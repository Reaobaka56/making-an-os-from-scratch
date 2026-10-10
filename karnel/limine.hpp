#pragma once
// Minimal Limine protocol definitions used by the memory manager.
#include <stdint.h>

#define LIMINE_REQ __attribute__((used, section(".limine_requests")))
#define LIMINE_MAGIC0 0xc7b1dd30df4c8b88ULL
#define LIMINE_MAGIC1 0x0a82e883a194f07bULL

enum : uint64_t {
    LIMINE_MEMMAP_USABLE = 0,
    LIMINE_MEMMAP_RESERVED = 1,
    LIMINE_MEMMAP_ACPI_RECLAIMABLE = 2,
    LIMINE_MEMMAP_ACPI_NVS = 3,
    LIMINE_MEMMAP_BAD_MEMORY = 4,
    LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE = 5,
    LIMINE_MEMMAP_EXECUTABLE_AND_MODULES = 6,
    LIMINE_MEMMAP_FRAMEBUFFER = 7,
};

struct LimineMemmapEntry { uint64_t base, length, type; };
struct LimineMemmapResponse { uint64_t revision, entry_count; LimineMemmapEntry** entries; };
struct LimineMemmapRequest { uint64_t id[4]; uint64_t revision; LimineMemmapResponse* response; };

struct LimineHhdmResponse { uint64_t revision, offset; };
struct LimineHhdmRequest { uint64_t id[4]; uint64_t revision; LimineHhdmResponse* response; };

struct LimineExecAddrResponse { uint64_t revision, physical_base, virtual_base; };
struct LimineExecAddrRequest { uint64_t id[4]; uint64_t revision; LimineExecAddrResponse* response; };
