#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define RGB565(r, g, b)                                                        \
  (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

void FrameBufferReset(void);
void FrameBufferSetPixel(uint16_t x, uint16_t y, uint16_t color);

#endif
