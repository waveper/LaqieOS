#ifndef GUIMAIN_H
#define GUIMAIN_H

#include <stdbool.h>
#include <stdint.h>

void GUIInit(void);
int AppendGUITask(int pid, uint16_t *pgm);
void RemoveCurrentGUITask(int pid);

#endif
