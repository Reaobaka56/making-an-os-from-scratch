#pragma once
#include <stddef.h>
void* malloc(size_t n);
void* calloc(size_t n, size_t sz);
void* realloc(void* p, size_t n);
void  free(void* p);
__attribute__((noreturn)) void exit(int code);
__attribute__((noreturn)) void abort(void);
int   atoi(const char* s);
long  strtol(const char* s, char** end, int base);
int   abs(int v);
