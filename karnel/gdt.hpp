#pragma once
void gdt_init();
#include <stdint.h>
void gdt_set_rsp0(uint64_t rsp0);   // kernel stack used on user->kernel transitions
