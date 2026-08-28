#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdarg.h>

// Print Null Termination
void printnt(const char *string) { prints(string, strlen(string)); }

void printpa(uintptr_t address) {
  static const char hex_digits[] = "0123456789ABCDEF";
  unsigned char i;
  // Print each 4-bit nibble in hexadecimal
  for (i = sizeof(address) * 2; i > 0; i--) {
    putchar(hex_digits[(address >> ((i - 1) * 4)) & 0xF]);
  }
}

// Helper function to reverse a string
static void reverse(char *str, int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}

// Custom implementation of itoa
static char *itoa(int value, char *str, int base) {
  unsigned int uvalue;
  int i = 0;
  int isNegative = 0;

  if (base < 2 || base > 36) {
    str[0] = '\0';
    return str;
  }

  if (value == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  if (value < 0 && base == 10) {
    isNegative = 1;
    uvalue = (unsigned int)(-(value + 1)) + 1;
  } else {
    uvalue = (unsigned int)value;
  }

  while (uvalue != 0) {
    unsigned int rem = uvalue % (unsigned int)base;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    uvalue /= (unsigned int)base;
  }

  if (isNegative) {
    str[i++] = '-';
  }

  str[i] = '\0';
  reverse(str, i);
  return str;
}

int vsnprintf(char *str, uint32_t size, const char *format, va_list args) {
  if (str == NULL || size == 0) {
    return 0;
  }

  size_t written = 0;

  for (size_t i = 0; format[i] != '\0'; i++) {
    // If we are out of space (leaving room for '\0'), stop writing but keep
    // counting to return the correct standard total length.
    if (format[i] == '%') {
      i++;
      if (format[i] == '\0') {
        if (written < size - 1) {
          str[written] = '%';
        }
        written++;
        break;
      }

      char pad = ' ';
      int width = 0;

      if (format[i] == '0') {
        pad = '0';
        i++;
      }

      while (format[i] >= '0' && format[i] <= '9') {
        width = width * 10 + (format[i] - '0');
        i++;
      }

      char *arg_str = NULL;
      char num_buf[34];
      char char_buf[2];

      if (format[i] == 'd') {
        int value = va_arg(args, int);
        arg_str = itoa(value, num_buf, 10);
      } else if (format[i] == 'x') {
        uint32_t value = va_arg(args, uint32_t);
        arg_str = itoa((int)value, num_buf, 16);
      } else if (format[i] == 's') {
        arg_str = va_arg(args, char *);
        if (arg_str == NULL) {
          arg_str = "(null)";
        }
      } else if (format[i] == 'c') {
        char c = (char)va_arg(args, int);
        char_buf[0] = c;
        char_buf[1] = '\0';
        arg_str = char_buf;
      } else if (format[i] == '%') {
        if (written < size - 1) {
          str[written] = '%';
        }
        written++;
        continue;
      } else {
        if (written < size - 1) {
          str[written] = '%';
        }
        written++;
        if (written < size - 1) {
          str[written] = format[i];
        }
        written++;
        continue;
      }

      if (arg_str != NULL) {
        int len = strlen(arg_str);

        // Process padding width
        while (len < width) {
          if (written < size - 1) {
            str[written] = pad;
          }
          written++;
          width--;
        }

        // Copy token contents
        while (*arg_str) {
          if (written < size - 1) {
            str[written] = *arg_str;
          }
          written++;
          arg_str++;
        }
      }
    } else {
      if (written < size - 1) {
        str[written] = format[i];
      }
      written++;
    }
  }

  // Always null-terminate safely within limits
  if (written < size) {
    str[written] = '\0';
  } else {
    str[size - 1] = '\0';
  }

  return (int)written;
}

void printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[4096];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  printnt(buffer);
}

/*
 * User-space heap allocator. The kernel owns a private, per-task region of
 * user-virtual memory (see kernel/layout.h USER_HEAP_*). This allocator obtains
 * page-sized arenas from the kernel via the SYS_KALLOC syscall and carves them
 * into smaller blocks with a first-fit free list. Freed blocks are recycled
 * locally; the kernel reclaims the underlying pages when the task exits.
 */
extern void *sys_kalloc(uint32_t size);
extern int sys_kfree(void *ptr);

typedef struct heap_block {
  struct heap_block *next;
  uint32_t size; /* usable payload size, excluding this header */
  uint8_t used;
} heap_block_t;

#define HEAP_HEADER_SIZE (((sizeof(heap_block_t) + 7) & ~(uintptr_t)7))
#define HEAP_ALIGN8(x) (((x) + 7U) & ~(uintptr_t)7U)

static heap_block_t *heap_free_list = NULL;

static heap_block_t *heap_grow(uint32_t need) {
  uint32_t req = need + HEAP_HEADER_SIZE;
  if (req < 4096U)
    req = 4096U;
  req = HEAP_ALIGN8(req);

  uint8_t *mem = (uint8_t *)sys_kalloc(req);
  if (!mem)
    return NULL;

  heap_block_t *block = (heap_block_t *)mem;
  block->size = req - HEAP_HEADER_SIZE;
  block->used = 0;
  block->next = heap_free_list;
  heap_free_list = block;
  return block;
}

void *malloc(uint32_t size) {
  if (size == 0)
    return NULL;

  uint32_t need = (uint32_t)HEAP_ALIGN8(size + HEAP_HEADER_SIZE);

  for (heap_block_t **pp = &heap_free_list; *pp; pp = &(*pp)->next) {
    heap_block_t *block = *pp;
    if (block->size < need)
      continue;

    /* Split the block if the remainder is large enough to be useful. */
    if (block->size >= need + HEAP_HEADER_SIZE + 8U) {
      heap_block_t *remainder = (heap_block_t *)((uint8_t *)block + need);
      remainder->size = block->size - need;
      remainder->used = 0;
      remainder->next = block->next;
      block->size = need;
      block->next = remainder;
    }

    *pp = block->next;
    block->used = 1;
    block->next = NULL;
    return (uint8_t *)block + HEAP_HEADER_SIZE;
  }

  heap_block_t *block = heap_grow(need);
  if (!block)
    return NULL;
  return malloc(size);
}

void free(void *ptr) {
  if (!ptr)
    return;
  heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEAP_HEADER_SIZE);
  block->used = 0;
  block->next = heap_free_list;
  heap_free_list = block;
}
