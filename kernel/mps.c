#include "../stdlib/stdmem.h"
#include "driver/ps2/mouse.h"
#include "page/bitmap.h"
#include "sched/sched.h"
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

typedef struct MousePointerData_t {
  uint16_t PointerX;
  uint16_t PointerY;
  bool MiddleButtonClicked;
  bool LeftButtonClicked;
  bool RightButtonClicked;
} MousePointerData_t;

static MousePointerData_t *MPD;
static int MPSTaskPid = 0;

void MousePointerService(void) {
  int MouseX = 0;
  int MouseY = 0;
  uint8_t MButtons = 0;
  while (1) {
    if (!MPD) {
      TaskKillCurrent();
      asm volatile("sti; hlt");
      continue;
    }

    MouseX = 0;
    MouseY = 0;
    MButtons = 0;
    PS2MouseFetch(&MouseX, &MouseY, &MButtons);

    if (MouseX == 0 && MouseY == 0 &&
        MPD->LeftButtonClicked == ((MButtons & MOUSE_LEFT_BUTTON) != 0) &&
        MPD->RightButtonClicked == ((MButtons & MOUSE_RIGHT_BUTTON) != 0) &&
        MPD->MiddleButtonClicked == ((MButtons & MOUSE_MIDDLE_BUTTON) != 0)) {
      asm volatile("sti; hlt");
      continue;
    }

    int nextX = (int)MPD->PointerX + MouseX;
    int nextY = (int)MPD->PointerY + MouseY;
    if (nextX < 0)
      nextX = 0;
    if (nextX >= SCREEN_WIDTH)
      nextX = SCREEN_WIDTH - 1;
    if (nextY < 0)
      nextY = 0;
    if (nextY >= SCREEN_HEIGHT)
      nextY = SCREEN_HEIGHT - 1;

    MPD->PointerX = (uint16_t)nextX;
    MPD->PointerY = (uint16_t)nextY;
    MPD->LeftButtonClicked = (MButtons & MOUSE_LEFT_BUTTON) != 0;
    MPD->RightButtonClicked = (MButtons & MOUSE_RIGHT_BUTTON) != 0;
    MPD->MiddleButtonClicked = (MButtons & MOUSE_MIDDLE_BUTTON) != 0;
  }
}

uint32_t MPSInit(void) {
  if (MPD)
    return (uint32_t)MPD;

  MPD = KAlloc(PAGE_SIZE);
  if (!MPD) {
    return 0;
  }
  memset(MPD, 0, PAGE_SIZE);

  MPSTaskPid = AppendTaskRing0("MPS", MousePointerService);
  if (MPSTaskPid < 0) {
    KFree(MPD);
    MPD = NULL;
    MPSTaskPid = 0;
    return 0;
  }

  return (uint32_t)MPD;
}

void MPSShutdown(void) {
  if (MPSTaskPid > 0) {
    TaskKillDeferred(MPSTaskPid);
    MPSTaskPid = 0;
  }
  if (MPD) {
    KFree(MPD);
    MPD = NULL;
  }
}
