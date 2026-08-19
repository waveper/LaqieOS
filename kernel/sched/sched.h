#ifndef SCHED_H
#define SCHED_H

#include "../page/paging.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct InterruptFrame {
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t gs, fs, es, ds;
  uint32_t eip, cs, eflags;
} InterruptFrame;

typedef struct UserInterruptFrame {
  InterruptFrame frame;
  uint32_t useresp;
  uint32_t ss;
} UserInterruptFrame;

void SCHEDInit(void);
int AppendTask(char *name, void (*start)(void));
int AppendTaskWithAddressSpace(char *name, void (*start)(void),
                               AddressSpace *address_space);
int AppendTaskRing0(char *name, void (*start)(void));
int TaskSetOwnedAllocation(int pid, void *allocation);
int TaskKillName(char *name);
void TaskKill(int pid);
bool IsTaskActive(int pid);
void TaskKillCurrent(void);
int TaskFetchID(void);
void ListTask(void);
void SchedMarkDead(void);
int CopyFromCurrentTaskUser(void *dst, const void *src, uint32_t size);
int CopyStringFromCurrentTaskUser(char *dst, uint32_t dst_size,
                                  const char *src);
int SchedGUITaskAppend(uint16_t *pgm);
InterruptFrame *Schedule(InterruptFrame *frame);

#endif
