#define IDT_ENTRIES 256

#include "../include/serial/serial.h"
#include "driver/floppy/floppy.h"
#include "driver/ps2/keyboard.h"
#include "driver/ps2/mouse.h"
#include "panic.h"
#include "sched/sched.h"
#include <stdbool.h>
#include <stdint.h>

#define cli() asm("cli");
#define sti() asm("sti");

extern void SendEoi(uint8_t irq);
extern void PageFaultISR(void);
extern void DivideByZeroISR(void);
extern void TimerISR(void);
extern void KeyboardISR(void);
extern void MouseISR(void);
extern void FloppyISR(void);
extern void SyscallISR(void);
extern void UnhandledInterruptISR(void);
extern void UnhandledErrorCodeISR(void);
extern void GeneralProtectionFaultISR(void);

static inline uint32_t read_cr2(void) {
  uint32_t val;
  __asm__ volatile("mov %%cr2, %0" : "=r"(val));
  return val;
}

typedef struct {
  int code;
  int a;
  int b;
  int c;
  int d;
} SysCall;

typedef struct idt_entry {
  uint16_t base_low;
  uint16_t selector;
  uint8_t always0;
  uint8_t flags;
  uint16_t base_high;
} __attribute__((packed)) idt_entry;

typedef struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) idt_ptr;

// stack arguments
typedef struct pagefaultstack {
  uint32_t edi, esi, ebp, esp_at_pushad, ebx, edx, ecx, eax;
  uint32_t gs, fs, es, ds;
  uint32_t error_code;
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t useresp;
  uint32_t ss;
} __attribute__((packed)) pagefaultstack;

idt_entry idt[256];

void DivideByZeroHandler(void) { Panic("You can't divide by zero, Silly!"); }

void UnhandledInterruptHandler(void) { Panic("Unhandled interrupt/exception"); }

void UnhandledErrorCodeHandler(void) {
  Panic("Unhandled exception with error code");
}

InterruptFrame *GeneralProtectionFaultHandler(pagefaultstack *regs) {
  InterruptFrame *frame = (InterruptFrame *)regs;

  int user = regs->cs & 0x3;

  if (user == 3) {
    SerialPrintf("Process ID: %d Was killed for Illegal opcode at program's "
                 "EIP: 0x%x\r\n",
                 TaskFetchID(), regs->eip);
    TaskKillCurrent();
    return Schedule(frame);
  } else {
    Panic("Kernel General Protection Fault????, idk how???");
  }

  return frame;
}

InterruptFrame *PageFaultHandler(pagefaultstack *regs) {
  uint32_t faulting_address = read_cr2();
  InterruptFrame *frame = (InterruptFrame *)regs;

  int present =
      !(regs->error_code & 0x1); // Page not present vs protection violation
  int write = regs->error_code & 0x2; // Write operation vs read operation
  int user =
      regs->error_code & 0x4; // Fault occurred in user mode vs supervisor mode
  int reserved = regs->error_code & 0x8; // Overwritten CPU reserved bits
  int id = regs->error_code & 0x10;      // Instruction fetch vs data access
  (void)reserved;
  (void)id;

  if (user) {
    SerialPrintf("Process ID: %d Was killed for ", TaskFetchID());
    SerialPrint(present ? "non-present page" : "protection violation");
    SerialPrint(", operation: ");
    SerialPrint(write ? "Writing" : "Reading");
    SerialPrintf(
        " (Segmentation Fault)\r\nAccessed Address: 0x%x, EIP: 0x%x\r\n",
        faulting_address, regs->eip);

    TaskKillCurrent();
    return Schedule(frame);
  } else {
    Panic("Kernel page fault");
  }

  return frame;
}

void KeyBoardInterruptHandler() {
  PS2KeyboardHandleIRQ();
  SendEoi(1);
}

void MouseInterruptHandler() {
  PS2MouseHandleIRQ();
  SendEoi(12);
}

void FloppyInterruptHandler() {
  FloppyHandleIRQ();
  SendEoi(6);
}

InterruptFrame *TimerInterruptHandler(InterruptFrame *frame) {
  frame = Schedule(frame);
  SendEoi(0);
  return frame;
}

void SetIDTEntry(int n, uint32_t handler, struct idt_entry *idt) {
  idt[n].base_low = handler & 0xFFFF;
  idt[n].selector = 0x08;
  idt[n].always0 = 0;
  idt[n].flags = 0x8E;
  idt[n].base_high = (handler >> 16) & 0xFFFF;
}

void SetIDTUserEntry(int n, uint32_t handler, struct idt_entry *idt) {
  idt[n].base_low = handler & 0xFFFF;
  idt[n].selector = 0x08;
  idt[n].always0 = 0;
  idt[n].flags = 0xEE;
  idt[n].base_high = (handler >> 16) & 0xFFFF;
}

void IDTInit(void) {
  struct idt_ptr idtp;
  cli();

  for (int i = 0; i < IDT_ENTRIES; ++i) {
    SetIDTEntry(i, (uint32_t)UnhandledInterruptISR, idt);
  }
  SetIDTEntry(0x08, (uint32_t)UnhandledErrorCodeISR, idt);
  SetIDTEntry(0x0A, (uint32_t)UnhandledErrorCodeISR, idt);
  SetIDTEntry(0x0B, (uint32_t)UnhandledErrorCodeISR, idt);
  SetIDTEntry(0x0C, (uint32_t)UnhandledErrorCodeISR, idt);
  SetIDTEntry(0x0D, (uint32_t)GeneralProtectionFaultISR, idt);
  SetIDTEntry(0x0E, (uint32_t)PageFaultISR, idt);
  SetIDTEntry(0x11, (uint32_t)UnhandledErrorCodeISR, idt);
  /*
   * User binaries use int 0x80 for the current syscall ABI, so expose that
   * gate to ring 3 even though the kernel still shares one flat address space.
   */
  SetIDTUserEntry(0x80, (uint32_t)SyscallISR, idt);
  SetIDTEntry(0x20, (uint32_t)TimerISR, idt);
  SetIDTEntry(0x21, (uint32_t)KeyboardISR, idt);
  SetIDTEntry(0x26, (uint32_t)FloppyISR, idt);
  SetIDTEntry(0x2C, (uint32_t)MouseISR, idt);
  SetIDTEntry(0x00, (uint32_t)DivideByZeroISR, idt);
  idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
  idtp.base = (uint32_t)idt;
  asm volatile("lidt (%0)" : : "r"(&idtp));
}
