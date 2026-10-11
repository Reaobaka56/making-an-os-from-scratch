#pragma once
#include <stddef.h>
#include <stdarg.h>

typedef struct { int fd; int eof; unsigned char rbuf[512]; int rpos, rlen; } FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)

int   printf(const char* fmt, ...);
int   fprintf(FILE* f, const char* fmt, ...);
int   snprintf(char* buf, size_t n, const char* fmt, ...);
int   sprintf(char* buf, const char* fmt, ...);
int   vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);
int   vfprintf(FILE* f, const char* fmt, va_list ap);
int   puts(const char* s);
int   putchar(int c);
int   getchar(void);
FILE* fopen(const char* path, const char* mode);
int   fclose(FILE* f);
int   fgetc(FILE* f);
int   fputc(int c, FILE* f);
int   fputs(const char* s, FILE* f);
char* fgets(char* buf, int n, FILE* f);
size_t fread(void* buf, size_t sz, size_t n, FILE* f);
size_t fwrite(const void* buf, size_t sz, size_t n, FILE* f);
