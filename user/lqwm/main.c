#include "../stdio.h"
#include "../stdlib.h"
#include <stdint.h>

extern uint32_t RequestFrameBuffer(void);
uint16_t *frame_buffer;

// TODO: work in progress GUI

int main(void) {
  frame_buffer = (uint16_t *)(uintptr_t)RequestFrameBuffer();
  if (!frame_buffer) {
    printnt("LQWM->ERROR: Cannot request for Frame Buffer\r\n");
    return 0;
  }
  printnt("LQWM->INFO: Window manager started\r\n");
  for (int i = 0; i < 307200; i++) {
    frame_buffer[i] = 0xB3;
  }
  return 0;
}
