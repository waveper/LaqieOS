#include "../include/serial/serial.h"
#include "../stdlib/stdmem.h"
#include "../stdlib/string.h"
#include "driver/apm/apm.h"
#include "driver/ps2/mouse.h"
#include "layout.h"
#include "page/kalloc.h"
#include "sched/sched.h"
#include <stdbool.h>
#include <stdint.h>

// Max Arguments
#define MARGS 16
typedef uint32_t size_t;

extern int MAX_ADDR;
extern uint8_t KernelEnd;

void SerialReadMax(char *buffer, unsigned int size) {
  unsigned int i = 0;
  char charbuffer;

  if (size == 0) {
    return;
  }

  // -1 to make space for null terminator
  while (i < size - 1) {
    charbuffer = SerialRead();
    if (charbuffer == 0x0D) {
      buffer[i] = '\0';
      SerialPrint("\r\n");
      return;
    } else if (charbuffer == 0x7F || charbuffer == 0x08) {
      if (i > 0) {
        --i;
        SerialPut('\b');
        SerialPut(' ');
        SerialPut('\b');
      }
    } else if (charbuffer >= ' ' && charbuffer <= '~') {
      buffer[i++] = charbuffer;
      SerialPut(charbuffer);
    }
  }
  buffer[i] = '\0';
  SerialPrint("\r\n");
}

int ParseArguments(const char *buffer, char argv[MARGS][64]) {
  int argc = 0;
  if (!buffer)
    return 0;

  while (*buffer && argc < MARGS) {
    int i = 0;
    while (*buffer == ' ')
      buffer++;
    if (*buffer == '\0')
      break;

    if (*buffer == '"') {
      for (++buffer; *buffer != '"' && *buffer && i < 63; ++buffer, ++i) {
        argv[argc][i] = (*buffer == '\\')
                            ? ((*(++buffer) == 'n') ? '\n' : *buffer)
                            : *buffer;
      }
      if (*buffer == '"')
        buffer++;
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

void KShellCommands(const char *string) {
  char argv[MARGS][64];
  memset(argv, 0, sizeof(argv));
  int argc = ParseArguments(string, argv);
  if (argc == 0) {
    return;
  }

  if (strcmp(argv[0], "meminfo") == 0) {
    SerialPrintf("Total RAM detected: %d KB\r\nUsable RAM detected: %d KB\r\n",
                 (MAX_ADDR + 0x100000) / 1024, MAX_ADDR / 1024);
    SerialPrint("Page Usage: ");
    uint32_t UsedKB =
        (MAX_ADDR / 1024) - ((CountAvailablePage() * PAGE_SIZE) / 1024);
    SerialPrintf("%d KB\r\n", UsedKB);
    SerialPrintf("Available RAM: %d KB\r\n",
                 (CountAvailablePage() * PAGE_SIZE) / 1024);
  } else if (strcmp(argv[0], "ps") == 0) {
    ListTask();
  } else if (strcmp(argv[0], "shutdown") == 0) {
    APMShutdown();
  } else {
    SerialPrint("Unknown command\r\n");
  }
}
