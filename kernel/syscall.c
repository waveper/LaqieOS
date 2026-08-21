#include "../include/serial/serial.h"
#include "fs/api.h"
#include "sched/exec.h"
#include "sched/sched.h"
#include <stdint.h>

// Serial I/O
#define SYS_PUTC 2
#define SYS_PRINTS 3
#define SYS_GETC 6

// tasking
#define SYS_EXEC 4
#define TASK_EXIT 1

// FRAME BUFFER ACCESS
#define SYS_REQ_FB 5

// Memory
#define SYS_KALLOC 7
#define SYS_KFREE 8
#define SYS_USERMAP 9

// File IO
#define SYS_FILESIZE 10
#define SYS_WRITEFILE 11
#define SYS_READFILE 12
#define SYS_DELFILE 13

#define SYSCALL_PATH_MAX 128

InterruptFrame *SyscallHandler(InterruptFrame *frame) {
  if (!frame)
    return frame;

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
      if (CopyFromCurrentTaskUser(
              &ch, (const void *)((uintptr_t)frame->ebx + (uintptr_t)i), 1) !=
          0) {
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
    if (CopyStringFromCurrentTaskUser(path, sizeof(path),
                                      (const char *)frame->ebx) != 0) {
      frame->eax = (uint32_t)-1;
      return frame;
    }
    frame->eax = (uint32_t)execute(path);
    return frame;
  }
  case SYS_REQ_FB:
    frame->eax = SchedREQFB();
    return frame;
  case SYS_GETC:
    frame->eax = SerialRead();
    return frame;
  case SYS_KALLOC:
    frame->eax = SchedTaskMalloc(frame->ebx);
    return frame;
  case SYS_KFREE:
    frame->eax = (uint32_t)SchedTaskFree(frame->ebx);
    return frame;
  case SYS_USERMAP:
    frame->eax = (uint32_t)SchedUserMMap(frame->ebx, frame->ecx, frame->edx);
    return frame;
  case SYS_FILESIZE:
    {
      char path[SYSCALL_PATH_MAX];
      if (CopyStringFromCurrentTaskUser(path, sizeof(path),
                                        (const char *)frame->ebx) != 0) {
        frame->eax = -1;
        return frame;
      }
      frame->eax = FileSize(path);
      return frame;
    }
  case SYS_WRITEFILE:
    frame->eax = -1;
    return frame;
  case SYS_READFILE:
    frame->eax = -1;
    return frame;
  case SYS_DELFILE: {
    char path[SYSCALL_PATH_MAX];
    if (CopyStringFromCurrentTaskUser(path, sizeof(path),
                                      (const char *)frame->ebx) != 0) {
      frame->eax = -1;
      return frame;
    }
    frame->eax = DeleteFile(path);
    return frame;
  }
  }

  frame->eax = (uint32_t)-1;
  return frame;
}
