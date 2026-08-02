#ifndef GUIMAIN_H
#define GUIMAIN_H

#include <stdint.h>
#include <stdbool.h>

#define GUI_TASK_NAME_MAX 32
#define GUI_LGM_SIZE      (80 * 24)

void GUIInit(void);
int AppendGUITask(int pid, const char *name, const uint8_t *lgm);
void RemoveGUITask(int pid);

#endif
