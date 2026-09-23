#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void IntToHex(int num, char *str);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
uint32_t hex_to_uint32(const char *hex_str);

#endif
