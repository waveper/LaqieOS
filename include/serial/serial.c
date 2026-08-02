#include "serial.h"

static void IntegerToHexadecimal(int value, char *buffer) {
  static const char hex_digits[] = "0123456789abcdef";
  for (int i = 7; i >= 0; i--) {
    buffer[i] = hex_digits[value & 0xF];
    value >>= 4;
  }
  buffer[8] = '\0';
}

void SerialInit(void)
{
        outb(SERIAL_IER, 0x00);  // Disable all interrupts
        outb(SERIAL_LCR, 0x80);  // Set DLAB (Data Access Bit) to 1
        outb(SERIAL_PORT, 0x03);  // Set LSB of baud rate divisor (e.g., 0x03 for 115200 baud)
        outb(SERIAL_IER, 0x00);  // Disable interrupts again (no need to enable them yet)
        outb(SERIAL_LCR, 0x03);  // LCR = 0x03 (8 bits, no parity, 1 stop bit)
        outb(SERIAL_PORT + 2, 0xC7);  // FIFO Control Register: Enable FIFO, clear RX/TX
        outb(SERIAL_MCR, 0x0B);  // Enable DTR and RTS for flow control
}

void SerialWaitForTransmit(void)
{
        while ((inb(SERIAL_LSR) & 0x20) == 0)
                ;
}

void SerialWaitForInput(void)
{
        while (!SerialCanRead())
                ;
}

void SerialPut(char c)
{
        SerialWaitForTransmit();
        outb(SERIAL_PORT, c);
}

bool SerialCanRead(void)
{
        return inb(SERIAL_PORT + 5) & 0x01;
}

char SerialRead(void)
{
        SerialWaitForInput();
        return inb(SERIAL_PORT);
}

// Basic form of printf
void SerialPrint(const char *string) {
  int i = 0;
  while (string[i] != '\0') {
    SerialPut(string[i]);
    i++;
  }
}

void SerialPrintHex(int value) {
  char hex_buffer[9];
  IntegerToHexadecimal(value, hex_buffer);
  for (int i = 0; i < 8; i++) {
    SerialPut(hex_buffer[i]);
  }
}

void SerialPrintPointerAddress(uintptr_t address) {
  static const char hex_digits[] = "0123456789ABCDEF";
  unsigned char i;
  // Print each 4-bit nibble in hexadecimal
  for (i = sizeof(address) * 2; i > 0; i--) {
    SerialPut(hex_digits[(address >> ((i - 1) * 4)) & 0xF]);
  }
}

void SerialPrintNum(int num) {
  // only handle positive number
  // Recursive case: If more than one digit, process the leading digits first
  if (num / 10) {
    SerialPrintNum(num / 10);
  }

  // ASCII manipulation stuff
  SerialPut((num % 10) + '0');
}
