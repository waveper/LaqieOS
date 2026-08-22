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
void TaskKillDeferred(int pid);
bool IsTaskActive(int pid);
void TaskKillCurrent(void);
int TaskFetchID(void);
void ListTask(void);
void SchedMarkDead(void);
int CopyFromCurrentTaskUser(void *dst, const void *src, uint32_t size);
int CopyStringFromCurrentTaskUser(char *dst, uint32_t dst_size,
                                  const char *src);
uint32_t SchedREQFB();
uint32_t SchedREQMouse();
uint32_t SchedTaskMalloc(uint32_t size);
int SchedTaskFree(uint32_t ptr);
int SchedUserMMap(uint32_t phys, uint32_t virt, uint32_t size);
InterruptFrame *Schedule(InterruptFrame *frame);

#endif
