#include <stdint.h>
#include <stdbool.h>
#include "../stdlib/stdmem.h"
#include "../include/serial/serial.h"
#include "../stdlib/string.h"
#include "page/kalloc.h"
#include "sched/sched.h"
#include "sched/exec.h"
#include "driver/ps2/mouse.h"
#include "driver/apm/apm.h"
#include "gui/main.h"
#include "panic.h"
#include "layout.h"

// Max Arguments
#define MARGS 16
typedef uint32_t size_t;

extern int MAX_ADDR;
extern uint8_t KernelEnd;

void SerialReadMax(char * buffer, unsigned int size) {
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

void KShellCommands(const char *string) {
  char argv[MARGS][64];
  memset(argv, 0, sizeof(argv));
  int argc = ParseArguments(string, argv);
  if (argc == 0) {
    return;
  }

  if (strcmp(argv[0], "meminfo") == 0) {
    SerialPrint("Total RAM detected: ");
    SerialPrintNum((MAX_ADDR + 0x100000) / 1024);
    SerialPrint(" KB\r\n");
    SerialPrint("Usable RAM detected: ");
    SerialPrintNum(MAX_ADDR / 1024);
    SerialPrint(" KB\r\n");
    SerialPrint("Page Usage: ");
    int UsedPages = CalculatePageUsage();
    SerialPrintNum(UsedPages / 1024);
    SerialPrint(" KB\r\n");
    SerialPrint("Available RAM: ");
    SerialPrintNum((MAX_ADDR - UsedPages) / 1024);
    SerialPrint(" KB\r\n");
  } else if (strcmp(argv[0], "memvisual") == 0) {
    SerialPrint("Memory Mapping Visual\r\n");
    SerialPrint("Undefined - 0 to 0x0000FFFF\r\n");
    SerialPrint("Kernel Binary - 0x00010000 to 0x");
    SerialPrintHex((int)&KernelEnd);
    SerialPrint("\r\nKernel Stack - 0x");
    SerialPrintHex((int)&KernelEnd + 1);
    SerialPrint(" to 0x");
    SerialPrintHex(KERNEL_STACK_TOP);
    SerialPrint("\r\n");
  } else if (strcmp(argv[0], "exit") == 0) {
    TaskKillName("shell");
  } else if (strcmp(argv[0], "ps") == 0) {
    ListTask();
  } else if (strcmp(argv[0], "shutdown") == 0) {
    APMShutdown();
  } else if (strcmp(argv[0], "exec") == 0) {
    if (argc < 2) {
      SerialPrint("Usage: exec <path>\r\n");
      return;
    }
    execute(argv[1]);
  } else {
    SerialPrint("Unknown command\r\n");
  }
}

void KShell(void) {
  char shellbuffer[64];
  while (1) {
    memset(shellbuffer, 0, sizeof(shellbuffer));
    SerialPrint("KShell: ");
    SerialReadMax(shellbuffer, 64);
    KShellCommands(shellbuffer);
  }
}
