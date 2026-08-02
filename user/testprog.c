#include <stdint.h>
#include "gui.h"

static uint8_t lgm[GUI_LOCAL_BUFFER_SIZE];

void main(void) {
  lgm[1] = 'H';
  lgm[2] = 'I';
  GUIInit("TestProg", lgm);
  while (1)
    ;
}
