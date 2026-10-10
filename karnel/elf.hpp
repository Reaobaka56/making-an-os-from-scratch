#pragma once
#include <stdint.h>
// Maps the PT_LOAD segments of a static ELF64 executable into the address space 'root'.
bool elf_load(uint64_t root, const uint8_t* image, uint64_t size, uint64_t* entry);
