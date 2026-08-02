#include <stdint.h>
#include "vga.h"
#include "../../../stdlib/stdmem.h"

// VGA Text mode

unsigned short* vga_buffer = (unsigned short*)0xB8000;
uint8_t row_offset = 0;
uint8_t column_offset = 0;
uint8_t vga_char_index = 0;

static void IntegerToHexadecimal(int value, char *buffer) {
  static const char hex_digits[] = "0123456789abcdef";
  for (int i = 7; i >= 0; i--) {
    buffer[i] = hex_digits[value & 0xF];
    value >>= 4;
  }
  buffer[8] = '\0';
}

void VGAInit(void) {
  memset(vga_buffer, 0, 4000); // clear the screen
}

void VGAReset(void) {
  memset(vga_buffer, 0, 4000);
  row_offset = 0;
  column_offset = 0;
  vga_char_index = 0;
}

void VGAPut(char c) {
  if (c == 0x0A) { // line feed
    row_offset++;
    vga_char_index = 0;
    return;
  }
  if (c == 0x08) {
    vga_char_index--;
    return;
  }
  vga_buffer[row_offset * 80 + column_offset + vga_char_index] = (unsigned short)0x0F00 | c;
  vga_char_index++;
}

void VGASetC(char c, int row, int column) {
  vga_buffer[row * 80 + column] = (unsigned short)0x0F00 | c;
}

// Basic form of printf
void VGAPrint(const char *string) {
  int i = 0;
  while (string[i] != '\0') {
    VGAPut(string[i]);
    i++;
  }
}

void VGAPrintHex(int value) {
  char hex_buffer[9];
  IntegerToHexadecimal(value, hex_buffer);
  for (int i = 0; i < 8; i++) {
    VGAPut(hex_buffer[i]);
  }
}

void VGAPrintPointerAddress(uintptr_t address) {
  static const char hex_digits[] = "0123456789ABCDEF";
  unsigned char i;
  // Print each 4-bit nibble in hexadecimal
  for (i = sizeof(address) * 2; i > 0; i--) {
    VGAPut(hex_digits[(address >> ((i - 1) * 4)) & 0xF]);
  }
}

void VGAPrintNum(int num) {
  // only handle positive number
  // Recursive case: If more than one digit, process the leading digits first
  if (num / 10) {
    VGAPrintNum(num / 10);
  }

  // ASCII manipulation stuff
  VGAPut((num % 10) + '0');
}
