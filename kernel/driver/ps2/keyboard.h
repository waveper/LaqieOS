#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include "generic.h"

uint8_t PS2KeyboardGetChar(void);
uint8_t PS2KeyboardFetch(volatile bool *hit);
bool    PS2KeyboardPresent(void);
void    PS2InitializeKeyboard(void);
int    PS2KeyboardInit(void);
void    PS2KeyboardHandleIRQ(void);

#endif
