#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void IntToHex(int num, char *str);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);

#endif
