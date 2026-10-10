#pragma once
#include <stdint.h>
#include "limine.hpp"

constexpr uint64_t VMM_PRESENT = 1ULL << 0;
constexpr uint64_t VMM_WRITE   = 1ULL << 1;
constexpr uint64_t VMM_USER    = 1ULL << 2;
constexpr uint64_t VMM_PWT     = 1ULL << 3;
constexpr uint64_t VMM_PCD     = 1ULL << 4;   // cache disable (MMIO)
constexpr uint64_t VMM_NX      = 1ULL << 63;

constexpr uint64_t USER_TOP = 0x0000800000000000ULL;  // first non-user address

void     vmm_init(LimineMemmapResponse* mm, LimineExecAddrResponse* exec);  // builds tables, loads CR3
bool     vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);             // kernel space, 4 KiB page
bool     vmm_unmap(uint64_t virt);                                          // does not free the frame
bool     vmm_translate(uint64_t virt, uint64_t* phys);

// Address spaces (for processes). The kernel half is shared between all of them.
uint64_t vmm_kernel_root();
uint64_t vmm_current_root();
uint64_t vmm_create_space();                                  // returns PML4 physical address, 0 on OOM
void     vmm_destroy_space(uint64_t root);                    // frees user-half tables and frames
bool     vmm_map_in(uint64_t root, uint64_t virt, uint64_t phys, uint64_t flags);
bool     vmm_translate_in(uint64_t root, uint64_t virt, uint64_t* phys);
void     vmm_switch(uint64_t root);
bool     vmm_user_ok(uint64_t virt, uint64_t len, bool write);  // is [virt, virt+len) user-accessible in the current space?

void*    mmio_map(uint64_t phys, uint64_t bytes);               // uncached mapping, returns virtual address
