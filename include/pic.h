#ifndef PIC_H
#define PIC_H
#include <stdint.h>
#include <stdbool.h>
#include "io.h"

#define PIC1_COMMAND     0x20
#define PIC1_DATA        0x21
#define PIC2_COMMAND     0xA0
#define PIC2_DATA        0xA1

void SendEoi(uint8_t irq) {
  if (irq >= 8) {
    outb(PIC2_COMMAND, 0x20);
  }
  outb(PIC1_COMMAND, 0x20);
}

void PicInit(void)
{
  /* initialize */
  outb(PIC1_COMMAND, 0x11);
  outb(PIC2_COMMAND, 0x11);
  outb(PIC1_DATA, 0x20);
  outb(PIC2_DATA, 0x28);
  outb(PIC1_DATA, 0x04);
  outb(PIC2_DATA, 0x02);
  outb(PIC1_DATA, 0x01);
  outb(PIC2_DATA, 0x01);
  outb(PIC1_DATA, 0x0);
  outb(PIC2_DATA, 0x0);
}

static inline void PicSetIRQMask(uint8_t irq) {
  uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
  uint8_t bit = (uint8_t)(1u << (irq & 7));
  outb(port, inb(port) | bit);
}

static inline void PicClearIRQMask(uint8_t irq) {
  uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
  uint8_t bit = (uint8_t)(1u << (irq & 7));
  outb(port, inb(port) & (uint8_t)~bit);
}

#endif
