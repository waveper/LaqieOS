extern void prints(const char *str, int len);

void main(void) {
  char message[11];
  message[0] = 'H';
  message[1] = 'e';
  message[2] = 'l';
  message[3] = 'l';
  message[4] = 'o';
  message[5] = ' ';
  message[6] = 'W';
  message[7] = 'o';
  message[8] = 'r';
  message[9] = 'l';
  message[10] = 'd';
  prints(message, sizeof(message));
}
