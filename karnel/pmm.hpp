#pragma once
#include <stdint.h>
#include "limine.hpp"

constexpr uint64_t PAGE_SIZE = 4096;

void     pmm_init(LimineMemmapResponse* mm);
uint64_t pmm_alloc();                 // returns physical address, 0 on OOM
void     pmm_free(uint64_t phys);
uint64_t pmm_free_frames();
uint64_t pmm_total_frames();          // usable frames at boot
