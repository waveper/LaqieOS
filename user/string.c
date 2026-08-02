#include <stdint.h>
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

void *memcpy(void *dest, const void *src, uint32_t size) {
  if (dest == NULL || src == NULL) return dest;

  uint8_t *dst_bytes = (uint8_t *)dest;
  const uint8_t *src_bytes = (const uint8_t *)src;
  while (size > 0 &&
         (((uintptr_t)dst_bytes & (sizeof(uint32_t) - 1)) != 0 ||
          ((uintptr_t)src_bytes & (sizeof(uint32_t) - 1)) != 0)) {
    *dst_bytes++ = *src_bytes++;
    --size;
  }

  uint32_t *dst_words = (uint32_t *)dst_bytes;
  const uint32_t *src_words = (const uint32_t *)src_bytes;
  while (size >= sizeof(uint32_t)) {
    *dst_words++ = *src_words++;
    size -= sizeof(uint32_t);
  }

  dst_bytes = (uint8_t *)dst_words;
  src_bytes = (const uint8_t *)src_words;
  while (size > 0) {
    *dst_bytes++ = *src_bytes++;
    --size;
  }

  return dest;
}

void memset(void* des, int value, uint32_t size) {
  if (des == NULL) return;
  uint8_t *dst_bytes = (uint8_t *)des;
  uint8_t byte_value = (uint8_t)value;

  while (size > 0 && ((uintptr_t)dst_bytes & (sizeof(uint32_t) - 1)) != 0) {
    *dst_bytes++ = byte_value;
    --size;
  }

  uint32_t word_value = (uint32_t)byte_value;
  word_value |= word_value << 8;
  word_value |= word_value << 16;

  uint32_t *dst_words = (uint32_t *)dst_bytes;
  while (size >= sizeof(uint32_t)) {
    *dst_words++ = word_value;
    size -= sizeof(uint32_t);
  }

  dst_bytes = (uint8_t *)dst_words;
  while (size > 0) {
    *dst_bytes++ = byte_value;
    --size;
  }
}
