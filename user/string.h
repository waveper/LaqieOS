#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void IntToHex(int num, char *str);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
void *memcpy(void *dest, const void *src, uint32_t size);
void memset(void* des, int value, uint32_t size);

#endif
