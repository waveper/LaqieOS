#include "stdio.h"
#include "stdlib.h"
#include <stdint.h>

int main(void) {
  prints("TA: start\r\n", 11);

  int *a = (int *)malloc(4096);
  int *b = (int *)malloc(4096);
  if (!a || !b) {
    prints("TA: kalloc failed\r\n", 18);
    return 1;
  }

  printnt("TA: a variable pointer address: ");
  printpa((uintptr_t)a);
  printnt("\r\nTA: b variable pointer address: ");
  printpa((uintptr_t)b);
  prints("\r\n", 2);

  *a = 10;
  *b = 20;
  if (*a != 10 || *b != 20) {
    prints("TA: readback failed\r\n", 20);
    return 2;
  }

  free(a);
  free(b);
  prints("TA: ok\r\n", 8);
  return 0;
}
