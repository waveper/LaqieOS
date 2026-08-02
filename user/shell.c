#include "stdio.h"
#include "string.h"
#include <stdint.h>

#define MARGS 8
#define EXEC_PREFIX "FD0:/"
#define EXEC_SUFFIX ".bin"
typedef uint32_t size_t;

void SerialReadMax(char * buffer, unsigned int size) {
  unsigned int i = 0;
  char charbuffer;

  if (size == 0) return;

  while (i < size - 1) {
    charbuffer = getchar();
    if (charbuffer == 0x0D) {
      buffer[i] = '\0';
      prints("\r\n", 2);
      return;
    } else if (charbuffer == 0x7F || charbuffer == 0x08) {
      if (i > 0) {
        --i;
        putchar('\b');
        putchar(' ');
        putchar('\b');
      }
    } else if (charbuffer >= ' ' && charbuffer <= '~') {
      buffer[i++] = charbuffer;
      putchar(charbuffer);
    }
  }
  buffer[i] = '\0';
  prints("\r\n", 2);
}

int ParseArguments(const char *buffer, char argv[MARGS][64]) {
  int argc = 0;
  if (!buffer) return 0;

  while (*buffer && argc < MARGS) {
    int i = 0;
    while (*buffer == ' ') buffer++;
    if (*buffer == '\0') break;

    if (*buffer == '"') {
      for (++buffer; *buffer != '"' && *buffer && i < 63; ++buffer, ++i) {
        argv[argc][i] = (*buffer == '\\') ? ((*(++buffer) == 'n') ? '\n' : *buffer) : *buffer;
      }
      if (*buffer == '"') buffer++;
    } else {
      for (; *buffer && *buffer != ' ' && i < 63; ++buffer, ++i) {
        argv[argc][i] = *buffer;
      }
    }

    argv[argc][i] = '\0';
    argc++;
  }

  return argc;
}

int main(void) {
  char shellbuffer[64];
  char execbuffer[80];
  char argv[MARGS][64];
  int argc = 0;
  while (1) {
    memset(shellbuffer, 0, sizeof(shellbuffer));
    memset(argv, 0, sizeof(argv));
    memset(execbuffer, 0, sizeof(execbuffer));
    prints("sh: ", 4);
    SerialReadMax(shellbuffer, 64);
    argc = ParseArguments(shellbuffer, argv);
    if (argc == 0) {
      continue;
    } else {
      unsigned int pos = 0;
      for (unsigned int i = 0; EXEC_PREFIX[i] != '\0' && pos < sizeof(execbuffer) - 1; ++i) {
        execbuffer[pos++] = EXEC_PREFIX[i];
      }
      for (unsigned int i = 0; argv[0][i] != '\0' && pos < sizeof(execbuffer) - 1; ++i) {
        execbuffer[pos++] = argv[0][i];
      }
      for (unsigned int i = 0; EXEC_SUFFIX[i] != '\0' && pos < sizeof(execbuffer) - 1; ++i) {
        execbuffer[pos++] = EXEC_SUFFIX[i];
      }
      execbuffer[pos] = '\0';
      exec(execbuffer);
    }
  }
}
