#include "fb.h"
#include "../string.h"
#include <stdint.h>

extern uint16_t *frame_buffer;
uint16_t screen_width = 640;
uint16_t screen_pitch = 1280; // Bytes per scanline (Width * 2 bytes/pixel)
// Convert 8-bit RGB components into a 16-bit RGB 565 value
#define RGB565(r, g, b)                                                        \
  (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

void FrameBufferReset(void) { memset(frame_buffer, 0, 614400); }

void FrameBufferSetPixel(uint16_t x, uint16_t y, uint16_t color) {
  if (x >= screen_width || y >= 480)
    return; // Bound checking
  uint32_t offset = (y * (screen_pitch / 2)) + x;
  frame_buffer[offset] = color;
}
/*
void draw_demo(void) {
  uint16_t red_color = RGB565(255, 0, 0);   // Pure Red
  uint16_t green_color = RGB565(0, 255, 0); // Pure Green

  // Draw a single red pixel at center of the screen
  FrameBufferSetPixel(320, 240, red_color);

  // Draw a horizontal green line
  for (uint16_t x = 100; x < 200; x++) {
    FrameBufferSetPixel(x, 100, green_color);
  }
}
*/
