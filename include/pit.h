#ifndef PIT_H
#define PIT_H
#include <stdint.h>
#include "io.h"

#define PIT_FREQUENCY 1193180
#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43
#define PIT_STATUS    0x61

void PITDelay(unsigned int milliseconds) {
  unsigned int count = 11932 * milliseconds;
  outb(PIT_COMMAND, 0x36);
  outb(PIT_CHANNEL0, count & 0xFF);
  outb(PIT_CHANNEL0, (count >> 8) & 0xFF);
  while ((inb(PIT_STATUS) & 0x80) == 0) {}
}

void PITInit(uint32_t frequency) {
  uint32_t divisor = 1193180 / frequency;
  outb(PIT_COMMAND, 0x36);
  outb(PIT_CHANNEL0, divisor & 0xFF);
  outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

#endif
