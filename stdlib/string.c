#include <stddef.h>
#include <stdint.h>

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
    if (*s1++ == '\0')
      return 0;
  }
  return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n > 0 && *s1 && *s2 && *s1 == *s2) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

size_t strlen(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

uint32_t hex_to_uint32(const char *hex_str) {
  if (hex_str == NULL)
    return 0;

  uint32_t result = 0;
  for (size_t i = 0; i < 8; i++) {
    char c = hex_str[i];

    // Break early if the string is shorter than 8 characters
    if (c == '\0')
      break;

    uint32_t value;

    if (c >= '0' && c <= '9') {
      value = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      value = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      value = c - 'a' + 10;
    } else {
      // Invalid hex character encountered
      return 0;
    }
    // Shift existing bits left by 4 to make room for the next nibble
    result = (result << 4) | value;
  }
  return result;
}
