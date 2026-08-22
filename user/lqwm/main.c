#include "../stdio.h"
#include "../stdlib.h"
#include "fb.h"
#include <stdbool.h>
#include <stdint.h>

extern uint32_t RequestFrameBuffer(void);
extern uint32_t RequestMousePointer(void);
uint16_t *frame_buffer;

#define BACKGROUND_COLOR RGB565(0, 0, 150)
#define POINTER_COLOR RGB565(255, 255, 255)

typedef struct MousePointerData_t {
  uint16_t PointerX;
  uint16_t PointerY;
  bool MiddleButtonClicked;
  bool LeftButtonClicked;
  bool RightButtonClicked;
} MousePointerData_t;

volatile MousePointerData_t *MPD;

// TODO: work in progress GUI

void DrawBackGround(void) {
  for (int i = 0; i < 307200; i++) {
    frame_buffer[i] = BACKGROUND_COLOR;
  }
}

int main(void) {
  frame_buffer = (uint16_t *)(uintptr_t)RequestFrameBuffer();
  if (!frame_buffer) {
    printnt("LQWM->ERROR: Cannot request for Frame Buffer\r\n");
    return 0;
  }
  printf("LQWM->DEBUG: Frame buffer address at 0x%x\r\n",
         (uint32_t)(uintptr_t)frame_buffer);
  MPD = (void *)(uintptr_t)RequestMousePointer();
  if (!MPD) {
    printnt("LQWM->ERROR: Cannot request for Mouse pointer access\r\n");
    return 0;
  }
  printf("LQWM->DEBUG: Mouse pointer data address at 0x%x\r\n",
         (uint32_t)(uintptr_t)MPD);
  printnt("LQWM->INFO: Window manager started\r\n");
  DrawBackGround();
  uint16_t last_x = MPD->PointerX;
  uint16_t last_y = MPD->PointerY;
  FrameBufferSetPixel(last_x, last_y, POINTER_COLOR);
  while (1) {
    uint16_t next_x = MPD->PointerX;
    uint16_t next_y = MPD->PointerY;
    if (next_x == last_x && next_y == last_y) {
      yield();
      continue;
    }

    FrameBufferSetPixel(last_x, last_y, BACKGROUND_COLOR);
    FrameBufferSetPixel(next_x, next_y, POINTER_COLOR);
    last_x = next_x;
    last_y = next_y;
  }
  return 0;
}
