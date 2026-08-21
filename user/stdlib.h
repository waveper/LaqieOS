#ifndef STDLIB
#define STDLIB

#include <stdarg.h>
#include <stdint.h>

#define NULL (void *)0

void printnt(const char *string);
void printpa(uintptr_t address);
int vsnprintf(char *str, uint32_t size, const char *format, va_list args);
void printf(const char *format, ...);

#endif
