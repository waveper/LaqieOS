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
