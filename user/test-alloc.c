#include "stdio.h"

int main(void) {
  prints("TA: start\r\n", 11);

  int *a = (int *)kalloc(4096);
  int *b = (int *)kalloc(4096);
  if (!a || !b) {
    prints("TA: kalloc failed\r\n", 18);
    return 1;
  }

  *a = 10;
  *b = 20;
  if (*a != 10 || *b != 20) {
    prints("TA: readback failed\r\n", 20);
    return 2;
  }

  kfree(a);
  kfree(b);
  prints("TA: ok\r\n", 8);
  return 0;
}