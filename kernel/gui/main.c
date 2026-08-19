#include "main.h"
#include "../driver/svga/svga.h"
#include "../page/kalloc.h"
#include "../sched/sched.h"
#include <stdbool.h>
#include <stdint.h>

// Simple GUI

extern unsigned short *svga_buffer;

typedef struct GUITask {
  int pid;
  uint16_t *pgm; // Program's Graphic Memory
} GUITask;

GUITask *CurrentGUITask;

int AppendGUITask(int pid, uint16_t *pgm) {
  if (!pid || !pgm)
    return -1;
  if (CurrentGUITask->pid != 0)
    return -1;
  CurrentGUITask->pid = pid;
  CurrentGUITask->pgm = pgm;
  return 0;
}

void RemoveCurrentGUITask(int pid) {
  if (CurrentGUITask->pid != pid)
    return;
  CurrentGUITask->pid = 0;
  CurrentGUITask->pgm = (void *)0;
}

void GUIMain(void) {
  while (1) {
    CopyFromCurrentTaskUser((void *)svga_buffer, CurrentGUITask->pgm, 614400);
  }
}

void GUIInit(void) {
  CurrentGUITask = kalloc(sizeof(GUITask));
  SVGAReset();
  AppendTaskRing0("GUI", GUIMain);
}
