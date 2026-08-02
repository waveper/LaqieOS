#include "../driver/ps2/mouse.h"
#include "../sched/sched.h"
#include "../page/kalloc.h"
#include "../driver/vga/vga.h"
#include "../../include/serial/serial.h"
#include "../../stdlib/stdmem.h"
#include "../../stdlib/string.h"
#include "main.h"
#include <stdbool.h>

// Text based GUI

extern void KShellCommands(const char *string);

static char *namedup(const char *name) {
  uint32_t len = 0;
  while (name[len] != '\0') {
    len++;
  }

  char *copy = kalloc(len + 1);
  if (!copy) return NULL;
  
  for (uint32_t i = 0; i <= len; ++i) {
    copy[i] = name[i];
  }

  return copy;
}

typedef struct GUITask {
  int pid;
  char *name;
  uint8_t *lgm; //local graphic memory
}GUITask;

// for SVGA 800 * 600
typedef struct GUIRequest {
  char *name;
  bool mode; // GUI mode, false is kernel assisted Font rendering, and true is program graphic rendering
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
  char *ltm; // local text memory
  uint16_t *lgm; // local graphic memory
}GUIRequest;

static GUITask *TaskList[64];
static uint32_t TaskListCount = 0;

static bool GUIStarted = false;
static bool PointerDrawn = false;
static bool PointerDirty = true;
static uint8_t PreviousPointerX = 0;
static uint8_t PreviousPointerY = 0;
static int TaskListIndex = -1;
static bool LastLeftButtonClicked = false;

// for 80 x 25 Screen
uint8_t PointerX = 0;
uint8_t PointerY = 0;
bool MiddleButtonClicked = false;
bool LeftButtonClicked = false;
bool RightButtonClicked = false;

static bool ValidTaskListIndex(void) {
  return TaskListIndex >= 0 && (uint32_t)TaskListIndex < TaskListCount &&
      TaskList[TaskListIndex] != NULL;
}

int AppendGUITask(int pid, const char *name, const uint8_t *lgm) {
  if (!pid || !name || !lgm) return -1;
  if (TaskListCount >= 64) return -1;

  int name_len = strlen(name);
  if (name_len > GUI_TASK_NAME_MAX) return -1;

  GUITask *new = kalloc(sizeof(GUITask));
  if (!new) return -1;

  new->pid = pid;
  new->name = namedup(name);
  new->lgm = kalloc(GUI_LGM_SIZE);
  if (!new->name || !new->lgm) {
    if (new->lgm) kfree(new->lgm);
    if (new->name) kfree(new->name);
    kfree(new);
    return -1;
  }
  memcpy(new->lgm, lgm, GUI_LGM_SIZE);

  TaskList[TaskListCount++] = new;
  return 0;
}

void RemoveGUITask(int pid) {
  for (uint32_t i = 0; i < TaskListCount; ++i) {
    GUITask *task = TaskList[i];
    if (!task || task->pid != pid) continue;

    kfree(task->name);
    kfree(task->lgm);
    kfree(task);

    for (uint32_t j = i + 1; j < TaskListCount; ++j) {
      TaskList[j - 1] = TaskList[j];
    }
    TaskList[--TaskListCount] = NULL;
    return;
  }
}

void MousePointerService(void) {
  int MouseX = 0;
  int MouseY = 0;
  uint8_t MButtons = 0;
  while (1) {
    MouseX = 0;
    MouseY = 0;
    MButtons = 0;
    PS2MouseFetch(&MouseX, &MouseY, &MButtons);

    if (MouseX == 0 && MouseY == 0 &&
        LeftButtonClicked == ((MButtons & MOUSE_LEFT_BUTTON) != 0) &&
        RightButtonClicked == ((MButtons & MOUSE_RIGHT_BUTTON) != 0) &&
        MiddleButtonClicked == ((MButtons & MOUSE_MIDDLE_BUTTON) != 0)) {
      asm volatile("sti; hlt");
      continue;
    }

    int nextX = (int)PointerX + MouseX;
    int nextY = (int)PointerY + MouseY;
    if (nextX < 0) nextX = 0;
    if (nextX >= SCREEN_WIDTH) nextX = SCREEN_WIDTH - 1;
    if (nextY < 0) nextY = 0;
    if (nextY >= SCREEN_HEIGHT) nextY = SCREEN_HEIGHT - 1;

    PointerX = (uint16_t)nextX;
    PointerY = (uint8_t)nextY;
    LeftButtonClicked = (MButtons & MOUSE_LEFT_BUTTON) != 0;
    RightButtonClicked = (MButtons & MOUSE_RIGHT_BUTTON) != 0;
    MiddleButtonClicked = (MButtons & MOUSE_MIDDLE_BUTTON) != 0;
    PointerDirty = true;
  }
}

void DrawMousePointer(void) {
  if (!PointerDirty) return;

  if (PointerDrawn) {
    char restore = (PreviousPointerY == SCREEN_HEIGHT - 1) ? (char)0xB2 : ' ';
    VGASetC(restore, PreviousPointerY, PreviousPointerX);
  }

  VGASetC((char)24, PointerY, PointerX);
  PreviousPointerX = PointerX;
  PreviousPointerY = PointerY;

  PointerDrawn = true;
  PointerDirty = false;
}

void RenderProgramLGM(void) {
  if (!ValidTaskListIndex()) return;

  for (int i = 0; i < SCREEN_HEIGHT - 1; ++i) {
    for (int j = 0; j < SCREEN_WIDTH; ++j) {
      if (i == PointerY && j == PointerX) continue;
      VGASetC(TaskList[TaskListIndex]->lgm[(i * SCREEN_WIDTH) + j], i, j);
    }
  }
}

void GUIMain(void) {
  while (1) {
    for (int i = 0; i < SCREEN_WIDTH; ++i) {
      if (i == PointerX && (SCREEN_HEIGHT - 1) == PointerY) {
        continue;
      }
      VGASetC((char)0xB2, SCREEN_HEIGHT - 1, i);
    }
    if (ValidTaskListIndex()) {
      for (int i = 0; i < 32; ++i) {
        if (TaskList[TaskListIndex]->name[i] == '\0') break;
        VGASetC(TaskList[TaskListIndex]->name[i], SCREEN_HEIGHT - 1, i);
      }
    }
    if (!(PointerX == (SCREEN_WIDTH - 1) && PointerY == (SCREEN_HEIGHT - 1))) {
      VGASetC('S', SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1);
    }
    if (!(PointerX == (SCREEN_WIDTH - 2) && PointerY == (SCREEN_HEIGHT - 1))) {
      VGASetC('_', SCREEN_HEIGHT - 1, SCREEN_WIDTH - 2);
    }
    if (!(PointerX == (SCREEN_WIDTH - 4) && PointerY == (SCREEN_HEIGHT - 1))) {
      VGASetC('>', SCREEN_HEIGHT - 1, SCREEN_WIDTH - 4);
    }
    if (!(PointerX == (SCREEN_WIDTH - 5) && PointerY == (SCREEN_HEIGHT - 1))) {
      VGASetC('<', SCREEN_HEIGHT - 1, SCREEN_WIDTH - 5);
    }
    RenderProgramLGM();
    DrawMousePointer();
    for (int i = 0; i < 50; ++i) asm volatile("nop");
  }
}

// Taskbar backend
void TaskBar(void) {
  while (1) {
    bool left_click = LeftButtonClicked && !LastLeftButtonClicked;
    LastLeftButtonClicked = LeftButtonClicked;

    if (!left_click) {
      asm volatile("sti; hlt");
      continue;
    }

    if ((PointerX == (SCREEN_WIDTH - 1) && PointerY == (SCREEN_HEIGHT - 1))) KShellCommands("shutdown");
    if ((PointerX == (SCREEN_WIDTH - 2) && PointerY == (SCREEN_HEIGHT - 1))) TaskListIndex = -1;
    if ((PointerX == (SCREEN_WIDTH - 4) && PointerY == (SCREEN_HEIGHT - 1))) {
      if (TaskListCount == 0) {
        TaskListIndex = -1;
      } else if (!ValidTaskListIndex()) {
        TaskListIndex = 0;
      } else {
        TaskListIndex++;
        if ((uint32_t)TaskListIndex >= TaskListCount) TaskListIndex = 0;
      }
    }
    if ((PointerX == (SCREEN_WIDTH - 5) && PointerY == (SCREEN_HEIGHT - 1))) {
      if (TaskListCount == 0) {
        TaskListIndex = -1;
      } else if (!ValidTaskListIndex()) {
        TaskListIndex = (int)TaskListCount - 1;
      } else if (TaskListIndex == 0) {
        TaskListIndex = (int)TaskListCount - 1;
      } else {
        TaskListIndex--;
      }
    }
  }
}

/*
The GUI has disabled for Redesigns and SVGA Implememtation
because I do not want a Plain CP437 font T-UI stuff, I want some Windows-like GUI
still, trying to figure out how to piece it together and Implement this
*/
// Another note: this GUI will be 640 * 480 16-bit resolution
void GUIInit(void) {
  SerialPrint("GUI disabled pending SVGA redesign\r\n");
  return;

  if (GUIStarted) return;

  VGAInit();
  
  AppendTaskRing0("MPS", MousePointerService);
  AppendTaskRing0("GUI", GUIMain);
  AppendTaskRing0("TaskBar", TaskBar);
  GUIStarted = true;
}
