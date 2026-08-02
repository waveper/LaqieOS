int main(void) {
  int *ptr = (void *)0; // test if the OS handle segmentation fault (page fault)
  *ptr = 10;
  return 0;
}
