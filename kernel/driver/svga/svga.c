#include <stdint.h>
#include "svga.h"
#include "../../../stdlib/stdmem.h"

// VBE 640 * 480 16-bit

unsigned short* svga_buffer = (unsigned short*)0;
uint16_t screen_width = 640;
uint16_t screen_pitch = 1280; // Bytes per scanline (Width * 2 bytes/pixel)
// Convert 8-bit RGB components into a 16-bit RGB 565 value
#define RGB565(r, g, b) (uint16_t)( \
  (((r) & 0xF8) << 8) | \
  (((g) & 0xFC) << 3) | \
  (((b) & 0xF8) >> 3) \
)

void SVGAInit(uint32_t VBE_ADRESS) {
  svga_buffer = (unsigned short *)VBE_ADRESS;
}

void SVGAReset(void) {
  if (!svga_buffer) return;
  memset(svga_buffer, 0, 614400);
}

void SVGASetPixel(uint16_t x, uint16_t y, uint16_t color) {
  if (!svga_buffer) return;
  if (x >= screen_width || y >= 480) return; // Bound checking
  uint32_t offset = (y * (screen_pitch / 2)) + x;
  svga_buffer[offset] = color;
}

void draw_demo(void) {
  uint16_t red_color = RGB565(255, 0, 0);     // Pure Red
  uint16_t green_color = RGB565(0, 255, 0);   // Pure Green

  // Draw a single red pixel at center of the screen
  SVGASetPixel(320, 240, red_color);

  // Draw a horizontal green line
  for (uint16_t x = 100; x < 200; x++) {
      SVGASetPixel(x, 100, green_color);
  }
}

