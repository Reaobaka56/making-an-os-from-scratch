#pragma once
#include <stdint.h>
#include "limine.hpp"

constexpr uint64_t VMM_PRESENT = 1ULL << 0;
constexpr uint64_t VMM_WRITE   = 1ULL << 1;
constexpr uint64_t VMM_USER    = 1ULL << 2;
constexpr uint64_t VMM_NX      = 1ULL << 63;

void vmm_init(LimineMemmapResponse* mm, LimineExecAddrResponse* exec);  // builds tables, loads CR3
bool vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);             // 4 KiB page
bool vmm_unmap(uint64_t virt);                                          // does not free the frame
bool vmm_translate(uint64_t virt, uint64_t* phys);
