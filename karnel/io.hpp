#pragma once
#include <stdint.h>

inline void outb(uint16_t port, uint8_t v) { asm volatile("outb %0, %1" :: "a"(v), "Nd"(port)); }
inline uint8_t inb(uint16_t port) { uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

inline void outw(uint16_t port, uint16_t v) { asm volatile("outw %0, %1" :: "a"(v), "Nd"(port)); }
inline uint16_t inw(uint16_t port) { uint16_t v; asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port)); return v; }
inline void outl(uint16_t port, uint32_t v) { asm volatile("outl %0, %1" :: "a"(v), "Nd"(port)); }
inline uint32_t inl(uint16_t port) { uint32_t v; asm volatile("inl %1, %0" : "=a"(v) : "Nd"(port)); return v; }
inline void insw(uint16_t port, void* buf, uint64_t count) { asm volatile("rep insw" : "+D"(buf), "+c"(count) : "d"(port) : "memory"); }
inline void outsw(uint16_t port, const void* buf, uint64_t count) { asm volatile("rep outsw" : "+S"(buf), "+c"(count) : "d"(port) : "memory"); }
inline void wrmsr(uint32_t msr, uint64_t v) { asm volatile("wrmsr" :: "a"((uint32_t)v), "d"((uint32_t)(v >> 32)), "c"(msr)); }
inline uint64_t rdmsr(uint32_t msr) { uint32_t lo, hi; asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr)); return ((uint64_t)hi << 32) | lo; }
