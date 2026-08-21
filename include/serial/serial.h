#ifndef SERIAL_H
#define SERIAL_H

#include "../io.h"
#include <stdint.h>

#define SERIAL_PORT 0x3F8
#define SERIAL_IER (SERIAL_PORT + 1) // Interrupt Enable Register
#define SERIAL_LCR (SERIAL_PORT + 3) // Line Control Register
#define SERIAL_LSR (SERIAL_PORT + 5) // Line Status Register
#define SERIAL_MCR (SERIAL_PORT + 4) // Modem Control Register

void SerialInit(void);
void SerialPut(char c);
void SerialWaitForTransmit(void);
void SerialWaitForInput(void);
bool SerialCanRead(void);
char SerialRead(void);
void SerialPrint(const char *string);
void SerialPrintPointerAddress(uintptr_t address);
void SerialPrintf(const char *format, ...);

#endif
