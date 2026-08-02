#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define GUI_LOCAL_BUFFER_SIZE (80 * 24)

int GUIInit(const char *name, const uint8_t *lgm);

#endif
