#include <stddef.h>

void IntToHex(int num, char *str) {
  static const char hex_digits[] = "0123456789abcdef";
  for (int i = 7; i >= 0; i--) {
    str[i] = hex_digits[num & 0xF];
    num >>= 4;
  }
  str[8] = '\0';
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 == *s2++) {
    if (*s1++ == '\0') return 0;
  }
  return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n > 0 && *s1 && *s2 && *s1 == *s2) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0) return 0;
  return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

size_t strlen(const char *s) {
  const char *p = s;
  while (*p) p++;
  return p - s;
}
