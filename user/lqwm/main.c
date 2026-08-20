#include <stdint.h>

extern uint32_t RequestFrameBuffer(void);
uint16_t *frame_buffer;

// TODO: work in progress GUI

int main(void) {
  frame_buffer = (uint16_t *)(uintptr_t)RequestFrameBuffer();
  for (int i = 0; i < 307200; i++) {
    frame_buffer[i] = 0xB3;
  }
  return 0;
}
