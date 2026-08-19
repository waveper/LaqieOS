#include "gui.h"
#include <stdint.h>

static uint16_t pgm[61440];

void main(void) {
  pgm[1] = 0x88;
  GUIInit(pgm);
  while (1)
    ;
}
