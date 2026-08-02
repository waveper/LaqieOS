#include "../include/serial/serial.h"
#include "sched/sched.h"
#include "sched/exec.h"
#include "gui/main.h"
#include <stdint.h>

// character I/O
#define SYS_PUTC 2
#define SYS_PRINTS 3
#define SYS_GETC 6

// tasking
#define SYS_EXEC 4
#define TASK_EXIT 1

// GUI
#define GUI_APPEND 5

#define SYSCALL_PATH_MAX 128

InterruptFrame *SyscallHandler(InterruptFrame *frame) {
  if (!frame) return frame;

  switch (frame->eax) {
    case TASK_EXIT:
      SchedMarkDead();
      return Schedule(frame);
    case SYS_PUTC:
      SerialPut(frame->ebx);
      frame->eax = 0;
      return frame;
    case SYS_PRINTS: {
      if (frame->ebx == 0 || (int32_t)frame->ecx < 0) {
        frame->eax = (uint32_t)-1;
        return frame;
      }
      if (frame->ecx == 0) {
        frame->eax = 0;
        return frame;
      }

      char ch;
      for (uint32_t i = 0; i < frame->ecx; i++) {
        if (CopyFromCurrentTaskUser(&ch, (const void *)((uintptr_t)frame->ebx + (uintptr_t)i), 1) != 0) {
          frame->eax = (uint32_t)-1;
          return frame;
        }
        SerialPut(ch);
      }
      frame->eax = 0;
      return frame;
    }
    case SYS_EXEC: {
      char path[SYSCALL_PATH_MAX];
      if (CopyStringFromCurrentTaskUser(path, sizeof(path), (const char *)frame->ebx) != 0) {
        frame->eax = (uint32_t)-1;
        return frame;
      }
      frame->eax = (uint32_t)execute(path);
      return frame;
    }
    case GUI_APPEND:
      frame->eax = (uint32_t)SchedGUITaskAppend((const char *)frame->ebx, (uint8_t *)frame->ecx);
      return frame;
    case SYS_GETC:
      frame->eax = SerialRead();
      return frame;
  }

  frame->eax = (uint32_t)-1;
  return frame;
}
