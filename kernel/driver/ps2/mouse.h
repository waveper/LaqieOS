#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H

#include "generic.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  uint8_t flags;
  int16_t x_movement;
  int16_t y_movement;
  int8_t z_movement;
  uint8_t buttons;
} ps2_mouse_packet_t;

#define MOUSE_LEFT_BUTTON (1 << 0)
#define MOUSE_RIGHT_BUTTON (1 << 1)
#define MOUSE_MIDDLE_BUTTON (1 << 2)
#define CURSOR_WIDTH 8
#define CURSOR_HEIGHT 10
#define MOUSE_STANDARD (0x00)
#define MOUSE_HAS_SCROLL (0x03)
#define MOUSE_HAS_5_BUTTONS (0x04)
#define PS2_CONFIG_PORT1_IRQ (1 << 0)
#define PS2_CONFIG_PORT2_IRQ (1 << 1)
#define PS2_CONFIG_PORT1_CLOCK_DISABLE (1 << 4)
#define PS2_CONFIG_PORT2_CLOCK_DISABLE (1 << 5)
#define PS2_STATUS_AUX_OUTPUT_FULL (1 << 5)

int PS2MouseInit(void);
void PS2MouseHandleIRQ(void);
void ps2_initialize_mouse(void);
bool ps2_read_mouse_packet(ps2_mouse_packet_t *packet);
void PS2MouseFetch(int *mx, int *my, uint8_t *buttons);
bool PS2MousePresent(void);
bool PS2MouseReady(void);

#endif
