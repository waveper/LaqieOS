#include <stdint.h>
#include <stddef.h>

extern uint32_t BootInstalledMemory;

// Return the BIOS-reported installed memory above 1 MiB.
int RamCountSize(void) {
  return (int)BootInstalledMemory;
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
