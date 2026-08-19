#include "gui/main.h"
#include "../include/pic.h"
#include "../include/pit.h"
#include "../include/serial/serial.h"
#include "../stdlib/stdmem.h"
#include "../stdlib/string.h"
#include "driver/floppy/floppy.h"
#include "driver/ps2/keyboard.h"
#include "driver/ps2/mouse.h"
#include "driver/svga/svga.h"
#include "layout.h"
#include "page/paging.h"
#include "panic.h"
#include "sched/exec.h"
#include "sched/sched.h"

#define cli() asm("cli");
#define sti() asm("sti");

extern void IDTInit(void);
extern void KShellCommands(const char *string);
extern void GDTInit(void);
extern uint8_t KernelEnd;
extern void draw_demo(void);

int MAX_ADDR;

_Noreturn void PanicImpl(const char *const file, long line,
                         const char *string) {
  cli();
  SerialPrint("\r\n KERNEL PANIC: ");
  SerialPrint(string);
  SerialPrint("\r\n");
  SerialPrint("At ");
  SerialPrint(file);
  SerialPut(':');
  SerialPrintNum((int)line);
  SerialPrint("\r\n");
  while (1)
    ;
}
void KMain(uint32_t VIDEO_ADRESS) {
  int floppy_status = -1;

  SerialInit();
  SVGAInit(VIDEO_ADRESS);
  if ((uintptr_t)&KernelEnd >= KERNEL_STACK_TOP) {
    Panic("Kernel image overlaps reserved kernel stack");
  }
  MAX_ADDR = RamCountSize();
  if ((MAX_ADDR + 0x100000) < 0x400000) {
    Panic("RAM should be more than 4MB");
  }
  GDTInit();
  IDTInit();
  PicInit();
  PITInit(100); // Initialize PIT at 100Hz
  PagingInit();
  if (VIDEO_ADRESS != 0 && PagingMapKernelRange(VIDEO_ADRESS, 614400) != 0) {
    Panic("Cannot map VBE framebuffer");
  }
  SCHEDInit();
  sti();

  SerialPrint("\r\nInitialized basic Kernel components\r\n");
  if (VIDEO_ADRESS == 0) {
    SerialPrint("VBE disabled or failed to initialize\r\n");
  } else {
    SerialPrint("VBE video address at: 0x");
    SerialPrintHex(VIDEO_ADRESS);
    SerialPrint("\r\n");
    draw_demo(); // Test GUI drawing. for future GUI
  }

  int ps2_keyboard_present = PS2KeyboardInit();
  int ps2_mouse_present = PS2MouseInit();
  if (!ps2_keyboard_present) {
    Panic("No PS2 keyboard found");
  }
  SerialPrint("PS2 Keyboard Initialized\r\n");
  if (ps2_mouse_present) {
    SerialPrint("PS2 Mouse Initialized\r\n");
  } else {
    SerialPrint("PS2 Mouse Not Detected\r\n");
  }

  floppy_status = FloppyInit();
  if (floppy_status == 0) {
    SerialPrint("Floppy Controller Initialized\r\n");
  } else {
    Panic("Floppy Controller Not Detected\r\n");
  }

  PicSetIRQMask(0);

  int user_shell_pid = execute("FD0:/shell.bin");
  if (user_shell_pid < 0) {
    Panic("Failed to launch user-space shell");
  }

  if (VIDEO_ADRESS != 0 && ps2_mouse_present) {
    SerialPrint("Launching kernel native GUI\r\n");
    execute("FD0:/testprog.bin");
    GUIInit();
  }

  KShellCommands("meminfo");

  PicClearIRQMask(0);
  while (1)
    asm volatile("sti; hlt");
}
