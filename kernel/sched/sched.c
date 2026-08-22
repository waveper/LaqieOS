#include "sched.h"
#include "../../include/serial/serial.h"
#include "../../stdlib/stdmem.h"
#include "../../stdlib/string.h"
#include "../layout.h"
#include "../page/kalloc.h"
#include "../page/paging.h"
#include <stdbool.h>
#include <stdint.h>

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR 0x1B
#define USER_DATA_SELECTOR 0x23

extern void TSSSetKernelStack(uint32_t esp0);
extern uint32_t MPSInit(void);
extern void MPSShutdown(void);

// A dynamic arrays of each tasks malloc allocation datas
typedef struct MallocAllocationData_t {
  uint32_t *size;
  uint32_t *pointers;
  uint32_t array_size;
} MallocAllocationData_t;

typedef struct Task {
  uint8_t stack[8192];
  InterruptFrame *frame;
  void *start;
  void *owned_allocation;
  void *user_stack_allocation;
  MallocAllocationData_t *malloc_allocation_data;
  AddressSpace *address_space;
  int tick;
  int pid;
  struct Task *next;
  bool running;
  char *name;
  int ring0;
} Task;

Task RootTask, *ActiveTask;
static Task *TaskTail = NULL;

static int WhichTaskGotAcessToFrameBuffer = 0;
static int WhichTaskGotAcessToMouse = 0;

#define IterateSchedule(_)                                                     \
  int _ = 0;                                                                   \
  for (Task *current = &RootTask; current != NULL; current = current->next, ++_)

/*
 * Records a kernel heap allocation so the owning task can free and revoke it
 * later. Grows the backing arrays when every slot is occupied. Returns the
 * (possibly new) descriptor, or NULL on allocation failure.
 */
static MallocAllocationData_t *
AppendTaskMallocAllocationData(MallocAllocationData_t *MAD, uint32_t address,
                               uint32_t size) {
  if (!MAD) {
    MAD = KAlloc(sizeof(MallocAllocationData_t));
    if (!MAD)
      return NULL;
    MAD->size = KAlloc(sizeof(uint32_t));
    MAD->pointers = KAlloc(sizeof(uint32_t));
    if (!MAD->size || !MAD->pointers) {
      if (MAD->pointers)
        KFree(MAD->pointers);
      if (MAD->size)
        KFree(MAD->size);
      KFree(MAD);
      return NULL;
    }
    MAD->array_size = 1;
    memset(MAD->size, 0, sizeof(uint32_t));
    memset(MAD->pointers, 0, sizeof(uint32_t));
  }

  for (uint32_t i = 0; i < MAD->array_size; i++) {
    if (MAD->size[i] == 0 && MAD->pointers[i] == 0) {
      MAD->pointers[i] = address;
      MAD->size[i] = size;
      return MAD;
    }
  }

  uint32_t new_size = MAD->array_size + 1;
  MallocAllocationData_t *MAD_NEW = KAlloc(sizeof(MallocAllocationData_t));
  if (!MAD_NEW)
    return NULL;
  MAD_NEW->size = KAlloc(new_size * sizeof(uint32_t));
  MAD_NEW->pointers = KAlloc(new_size * sizeof(uint32_t));
  if (!MAD_NEW->size || !MAD_NEW->pointers) {
    if (MAD_NEW->pointers)
      KFree(MAD_NEW->pointers);
    if (MAD_NEW->size)
      KFree(MAD_NEW->size);
    KFree(MAD_NEW);
    return NULL;
  }
  memset(MAD_NEW->size, 0, new_size * sizeof(uint32_t));
  memset(MAD_NEW->pointers, 0, new_size * sizeof(uint32_t));
  memcpy(MAD_NEW->size, MAD->size, MAD->array_size * sizeof(uint32_t));
  memcpy(MAD_NEW->pointers, MAD->pointers, MAD->array_size * sizeof(uint32_t));

  MAD_NEW->array_size = new_size;
  MAD_NEW->pointers[MAD->array_size] = address;
  MAD_NEW->size[MAD->array_size] = size;

  KFree(MAD->pointers);
  KFree(MAD->size);
  KFree(MAD);
  return MAD_NEW;
}

/* Marks a tracked allocation entry as free without touching the memory. */
static void RemoveTaskMallocAllocation(Task *task, uint32_t ptr) {
  if (!task)
    return;
  MallocAllocationData_t *MAD = task->malloc_allocation_data;
  if (!MAD)
    return;
  for (uint32_t i = 0; i < MAD->array_size; i++) {
    if (MAD->pointers[i] == ptr && MAD->size[i] != 0) {
      MAD->pointers[i] = 0;
      MAD->size[i] = 0;
      return;
    }
  }
}

/* True if every page in [addr, addr+size) falls inside a tracked allocation. */
static bool TaskOwnsMallocRange(Task *task, uintptr_t addr, uint32_t size) {
  if (!task || task->ring0)
    return false;
  MallocAllocationData_t *MAD = task->malloc_allocation_data;
  if (!MAD)
    return false;

  uint32_t start = (uint32_t)addr & ~(uint32_t)(PAGE_SIZE - 1);
  uint32_t end =
      ((uint32_t)addr + size + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);

  for (uint32_t page = start; page < end; page += PAGE_SIZE) {
    bool covered = false;
    for (uint32_t i = 0; i < MAD->array_size; i++) {
      if (MAD->pointers[i] == 0 || MAD->size[i] == 0)
        continue;
      uint32_t alloc_start = MAD->pointers[i];
      uint32_t alloc_end = MAD->pointers[i] + MAD->size[i];
      if (page >= alloc_start && page < alloc_end) {
        covered = true;
        break;
      }
    }
    if (!covered)
      return false;
  }
  return true;
}

/* Frees every tracked heap allocation plus the tracking metadata itself. */
static void FreeTaskMallocAllocations(Task *task) {
  if (!task)
    return;
  MallocAllocationData_t *MAD = task->malloc_allocation_data;
  if (!MAD)
    return;

  for (uint32_t i = 0; i < MAD->array_size; i++) {
    if (MAD->pointers[i] != 0 && MAD->size[i] != 0) {
      KFree((void *)MAD->pointers[i]);
    }
  }
  KFree(MAD->pointers);
  KFree(MAD->size);
  KFree(MAD);
  task->malloc_allocation_data = NULL;
}

/*
 * Duplicates a task name into heap memory owned by the scheduler so task
 * metadata stays valid after the caller returns.
 */
static char *TaskNameDup(const char *name) {
  uint32_t len = 0;
  while (name[len] != '\0') {
    len++;
  }

  char *copy = kalloc(len + 1);
  if (!copy)
    return NULL;

  for (uint32_t i = 0; i <= len; ++i) {
    copy[i] = name[i];
  }

  return copy;
}

static void DestroyTask(Task *task) {
  if (!task || task == &RootTask)
    return;
  if (WhichTaskGotAcessToFrameBuffer == task->pid) {
    WhichTaskGotAcessToFrameBuffer = 0;
  }
  if (WhichTaskGotAcessToMouse == task->pid) {
    MPSShutdown();
    WhichTaskGotAcessToMouse = 0;
  }
  FreeTaskMallocAllocations(task);
  if (task->owned_allocation) {
    kfree(task->owned_allocation);
  }
  if (task->user_stack_allocation) {
    kfree(task->user_stack_allocation);
  }
  if (task->address_space &&
      task->address_space != PagingKernelAddressSpace()) {
    PagingDestroyAddressSpace(task->address_space);
  }
  if (task->name) {
    kfree(task->name);
  }
  kfree(task);
}

static uint32_t TaskKernelStackTop(Task *task) {
  return (uint32_t)&task->stack[sizeof(task->stack)];
}

static bool RangeContains(uintptr_t start, uintptr_t limit, uintptr_t addr,
                          uint32_t size) {
  if (addr < start || addr >= limit)
    return false;
  if (size == 0)
    return true;

  uintptr_t end = addr + (uintptr_t)size;
  if (end < addr)
    return false;
  return end <= limit;
}

static bool TaskOwnsUserRange(Task *task, uintptr_t addr, uint32_t size) {
  if (!task || task->ring0)
    return false;

  if (RangeContains(USER_EXEC_LOAD_ADDR, USER_EXEC_END, addr, size)) {
    return true;
  }

  if (task->address_space &&
      task->address_space != PagingKernelAddressSpace()) {
    return RangeContains(USER_STACK_BASE, USER_STACK_TOP, addr, size);
  }

  if (!task->user_stack_allocation) {
    return false;
  }

  return RangeContains((uintptr_t)task->user_stack_allocation,
                       (uintptr_t)task->user_stack_allocation + USER_STACK_SIZE,
                       addr, size);
}

static bool IsFixedUserRange(uintptr_t addr, uint32_t size) {
  return RangeContains(USER_EXEC_LOAD_ADDR, USER_EXEC_END, addr, size) ||
         RangeContains(USER_STACK_BASE, USER_STACK_TOP, addr, size);
}

static void SchedReapDeadTasks(void) {
  Task *prev = &RootTask;
  Task *current = RootTask.next;

  while (current) {
    if (!current->running && current != ActiveTask) {
      Task *next = current->next;
      prev->next = next;
      if (TaskTail == current) {
        TaskTail = prev;
      }
      DestroyTask(current);
      current = next;
      continue;
    }
    prev = current;
    current = current->next;
  }
}

/*
 * Stops the current CPU forever. The scheduler uses this when the active task
 * has been killed and there is no safe continuation path for that context.
 */
void halt(void) {
  while (true) {
    asm volatile("sti; hlt");
  }
}

/*
 * Initializes the scheduler root task, which represents the boot kernel
 * context before the first scheduled task is dispatched.
 */
void SCHEDInit(void) {
  memset(&RootTask, 0, sizeof(Task));
  RootTask.ring0 = true;
  RootTask.next = NULL;
  RootTask.running = true;
  RootTask.pid = 0;
  RootTask.name = "SCHED";
  RootTask.address_space = PagingKernelAddressSpace();
  TaskTail = &RootTask;
  ActiveTask = &RootTask;
}

/*
 * Allocates a task descriptor, duplicates its name, links it into the run
 * list, and assigns a PID. The caller decides whether the task runs in ring 0
 * or ring 3 and which initial CPU frame it receives.
 */
Task *CreateTask(char *name, void (*start)(void)) {
  static int pid = 1;
  if (!name || !start)
    return NULL;
  Task *new = kalloc(sizeof(Task));
  if (!new)
    return NULL;
  memset(new, 0, sizeof(Task));
  new->name = TaskNameDup(name);
  if (!new->name) {
    kfree(new);
    return NULL;
  }
  new->next = NULL;
  TaskTail->next = new;
  TaskTail = new;
  new->ring0 = false;
  new->start = (void *)start;
  new->owned_allocation = NULL;
  new->user_stack_allocation = NULL;
  new->address_space = PagingKernelAddressSpace();
  new->running = false;
  new->pid = pid++;
  return new;
}

/*
 * Builds the saved interrupt frame expected by TimerISR for a kernel task.
 * When the scheduler later returns this frame, iretd resumes directly in ring
 * 0 at the task entry point.
 */
static InterruptFrame *SetupRing0Frame(Task *task) {
  InterruptFrame *frame =
      (InterruptFrame *)&task
          ->stack[sizeof(task->stack) - sizeof(InterruptFrame)];
  memset(frame, 0, sizeof(InterruptFrame));
  frame->ds = KERNEL_DATA_SELECTOR;
  frame->es = KERNEL_DATA_SELECTOR;
  frame->fs = KERNEL_DATA_SELECTOR;
  frame->gs = KERNEL_DATA_SELECTOR;
  frame->eip = (uint32_t)task->start;
  frame->cs = KERNEL_CODE_SELECTOR;
  frame->eflags = 0x200;
  return frame;
}

/*
 * Builds the saved interrupt frame expected by TimerISR for a user task.
 * iretd will restore ring 3 selectors and switch to the task's user stack.
 */
static InterruptFrame *SetupRing3Frame(Task *task) {
  if (!task->address_space ||
      task->address_space == PagingKernelAddressSpace()) {
    if (!task->user_stack_allocation) {
      task->user_stack_allocation = kalloc(USER_STACK_SIZE);
      if (!task->user_stack_allocation)
        return NULL;
    }
  }

  UserInterruptFrame *frame =
      (UserInterruptFrame *)&task
          ->stack[sizeof(task->stack) - sizeof(UserInterruptFrame)];
  memset(frame, 0, sizeof(UserInterruptFrame));
  frame->frame.ds = USER_DATA_SELECTOR;
  frame->frame.es = USER_DATA_SELECTOR;
  frame->frame.fs = USER_DATA_SELECTOR;
  frame->frame.gs = USER_DATA_SELECTOR;
  frame->frame.eip = (uint32_t)task->start;
  frame->frame.cs = USER_CODE_SELECTOR;
  frame->frame.eflags = 0x200;
  frame->useresp = task->address_space == PagingKernelAddressSpace()
                       ? (uint32_t)task->user_stack_allocation + USER_STACK_SIZE
                       : USER_STACK_TOP;
  frame->ss = USER_DATA_SELECTOR;
  return &frame->frame;
}

/*
 * Creates a ring 3 task with a dedicated kernel interrupt stack and user
 * stack so privilege transitions have stable state to save and restore.
 */
int AppendTask(char *name, void (*start)(void)) {
  Task *tsk = CreateTask(name, start);
  if (!tsk)
    return -1;
  tsk->ring0 = false;
  tsk->frame = SetupRing3Frame(tsk);
  if (!tsk->frame) {
    TaskTail = &RootTask;
    for (Task *current = &RootTask; current && current->next;
         current = current->next) {
      if (current->next == tsk) {
        current->next = NULL;
        TaskTail = current;
        break;
      }
    }
    DestroyTask(tsk);
    return -1;
  }
  tsk->running = true;
  return tsk->pid;
}

int AppendTaskWithAddressSpace(char *name, void (*start)(void),
                               AddressSpace *address_space) {
  Task *tsk = CreateTask(name, start);
  if (!tsk)
    return -1;
  tsk->ring0 = false;
  tsk->address_space = address_space;
  tsk->frame = SetupRing3Frame(tsk);
  if (!tsk->frame) {
    TaskTail = &RootTask;
    for (Task *current = &RootTask; current && current->next;
         current = current->next) {
      if (current->next == tsk) {
        current->next = NULL;
        TaskTail = current;
        break;
      }
    }
    tsk->address_space = NULL;
    DestroyTask(tsk);
    return -1;
  }
  tsk->running = true;
  return tsk->pid;
}

/*
 * Creates a ring 0 task. These tasks keep kernel selectors and return to the
 * entry point without a privilege transition.
 */
int AppendTaskRing0(char *name, void (*start)(void)) {
  Task *tsk = CreateTask(name, start);
  if (!tsk)
    return -1;
  tsk->ring0 = true;
  tsk->frame = SetupRing0Frame(tsk);
  tsk->running = true;
  return tsk->pid;
}

int TaskSetOwnedAllocation(int pid, void *allocation) {
  IterateSchedule(_) {
    if (current && current->pid == pid) {
      current->owned_allocation = allocation;
      return 0;
    }
  }
  return -1;
}

/* Mark current task as dead */
void SchedMarkDead(void) {
  if (!ActiveTask || ActiveTask == &RootTask)
    return;
  ActiveTask->running = false;
}

/*
 * Stops the first task whose name matches. If the current task kills itself,
 * execution is halted because its context is no longer valid to resume.
 */
int TaskKillName(char *name) {
  IterateSchedule(id) {
    if (current && strcmp(current->name, name) == 0) {
      current->running = false;
      if (current == ActiveTask) {
        halt();
      }
      return current->pid;
    }
  }
  return -1;
}

void TaskKillCurrent(void) {
  if (!ActiveTask || ActiveTask == &RootTask)
    return;
  SchedMarkDead();
}

int TaskFetchID(void) { return ActiveTask ? ActiveTask->pid : -1; }

void TaskKillDeferred(int pid) {
  IterateSchedule(_) {
    if (current && current->pid == pid) {
      current->running = false;
      return;
    }
  }
}

/*
 * Stops a task by PID. Killing the currently running task halts immediately to
 * avoid returning into a dead context.
 */
void TaskKill(int pid) {
  IterateSchedule(aaa) {
    if (current && current->pid == pid) {
      current->running = false;
      if (current == ActiveTask) {
        halt();
      }
    }
  }
}

extern uint32_t FRAME_BUFFER_ADDRESS;

uint32_t SchedREQFB(void) {
  if (FRAME_BUFFER_ADDRESS == 0 || WhichTaskGotAcessToFrameBuffer != 0)
    return 0;
  if (PagingMapUserPhysicalRange(ActiveTask->address_space,
                                 FRAME_BUFFER_ADDRESS, FRAME_BUFFER_ADDRESS,
                                 614400, 1) == -1)
    return 0;
  WhichTaskGotAcessToFrameBuffer = ActiveTask->pid;
  return FRAME_BUFFER_ADDRESS;
}

uint32_t SchedREQMouse(void) {
  if (WhichTaskGotAcessToMouse != 0)
    return 0;
  uint32_t MPSDataPtr = MPSInit();
  if (!MPSDataPtr)
    return 0;
  if (PagingMapUserPhysicalRange(ActiveTask->address_space, MPSDataPtr,
                                 MPSDataPtr, 4096, 1)) {
    MPSShutdown();
    return 0;
  }
  WhichTaskGotAcessToMouse = ActiveTask->pid;
  return MPSDataPtr;
}

// Allocates memory for the active (user) task, maps it into its address
// space, and tracks it so it can be freed and revoked on exit.
uint32_t SchedTaskMalloc(uint32_t size) {
  if (!ActiveTask || !size)
    return 0;
  uint32_t ptr = (uint32_t)KAlloc(size);
  if (!ptr)
    return 0;
  MallocAllocationData_t *tracked = AppendTaskMallocAllocationData(
      ActiveTask->malloc_allocation_data, ptr, size);
  if (!tracked) {
    KFree((void *)ptr);
    return 0;
  }
  ActiveTask->malloc_allocation_data = tracked;
  if (PagingMapUserPhysicalRange(ActiveTask->address_space, ptr, ptr, size,
                                 1) != 0) {
    RemoveTaskMallocAllocation(ActiveTask, ptr);
    KFree((void *)ptr);
    return 0;
  }
  return ptr;
}

// Frees a tracked allocation owned by the active task. The mapping is revoked
// before the pages return to the kernel heap so user code cannot touch freed
// memory.
int SchedTaskFree(uint32_t ptr) {
  if (!ActiveTask || !ptr)
    return -1;
  MallocAllocationData_t *MAD = ActiveTask->malloc_allocation_data;
  if (!MAD)
    return -1;

  uint32_t size = 0;
  for (uint32_t i = 0; i < MAD->array_size; i++) {
    if (MAD->pointers[i] == ptr && MAD->size[i] != 0) {
      size = MAD->size[i];
      break;
    }
  }
  if (!size)
    return -1;

  if (PagingUnmapUserRange(ActiveTask->address_space, ptr, size) != 0)
    return -1;
  KFree((void *)ptr);
  RemoveTaskMallocAllocation(ActiveTask, ptr);
  return 0;
}

// Maps physical memory owned by the active task into its address space.
int SchedUserMMap(uint32_t phys, uint32_t virt, uint32_t size) {
  if (!ActiveTask || !phys || !virt || !size)
    return -1;
  if (!TaskOwnsMallocRange(ActiveTask, phys, size))
    return -1;
  if (PagingMapUserPhysicalRange(ActiveTask->address_space, virt, phys, size,
                                 1) != 0)
    return -1;
  return 0;
}

/*
 * Reports whether a PID is still marked runnable inside the scheduler list.
 */
bool IsTaskActive(int pid) {
  IterateSchedule(_) {
    if (current && current->pid == pid)
      return current->running;
  }
  return false;
}

/*
 * Dumps the scheduler task list to the serial console for shell debugging.
 */
void ListTask(void) {
  IterateSchedule(i) {
    if (current) {
      SerialPrintf("%s:%d ", current->name, current->pid);
    }
  }
  SerialPrint("\r\n");
}

int CopyFromCurrentTaskUser(void *dst, const void *src, uint32_t size) {
  if (!dst || !src)
    return -1;
  if (!TaskOwnsUserRange(ActiveTask, (uintptr_t)src, size) &&
      !IsFixedUserRange((uintptr_t)src, size)) {
    return -1;
  }
  memcpy(dst, src, size);
  return 0;
}

int CopyStringFromCurrentTaskUser(char *dst, uint32_t dst_size,
                                  const char *src) {
  if (!dst || !src || dst_size == 0)
    return -1;

  for (uint32_t i = 0; i < dst_size; ++i) {
    if (!TaskOwnsUserRange(ActiveTask, (uintptr_t)(src + i), 1) &&
        !IsFixedUserRange((uintptr_t)(src + i), 1)) {
      return -1;
    }

    dst[i] = src[i];
    if (dst[i] == '\0') {
      return 0;
    }
  }

  dst[dst_size - 1] = '\0';
  return -1;
}

/*
 * Advances to the next runnable task in the list, wrapping back to the root
 * scheduler context when it reaches the end.
 */
void SchedNext() {
  Task *candidate = ActiveTask;

  for (;;) {
    if (candidate && candidate->next) {
      candidate = candidate->next;
    } else {
      candidate = &RootTask;
    }

    if (candidate->running) {
      ActiveTask = candidate;
      TSSSetKernelStack(TaskKernelStackTop(ActiveTask));
      PagingSwitchAddressSpace(ActiveTask->address_space);
      return;
    }

    if (candidate == ActiveTask) {
      ActiveTask = &RootTask;
      TSSSetKernelStack(TaskKernelStackTop(ActiveTask));
      PagingSwitchAddressSpace(ActiveTask->address_space);
      return;
    }
  }
}

/*
 * Saves the interrupted context into the current task, picks the next runnable
 * task, lazily creates its initial CPU frame if needed, and returns the frame
 * TimerISR should restore.
 */
InterruptFrame *Schedule(InterruptFrame *frame) {
  asm volatile("cli");
  if (!ActiveTask || !frame)
    return frame;

  ActiveTask->frame = frame;
  ActiveTask->tick++;
  SchedReapDeadTasks();

  SchedNext();
  if (!ActiveTask->frame) {
    ActiveTask->frame = ActiveTask->ring0 ? SetupRing0Frame(ActiveTask)
                                          : SetupRing3Frame(ActiveTask);
    if (!ActiveTask->frame) {
      ActiveTask->running = false;
      ActiveTask = &RootTask;
      TSSSetKernelStack(TaskKernelStackTop(ActiveTask));
      PagingSwitchAddressSpace(ActiveTask->address_space);
      if (!ActiveTask->frame) {
        halt();
      }
      return ActiveTask->frame;
    }
  }

  return ActiveTask->frame;
}
