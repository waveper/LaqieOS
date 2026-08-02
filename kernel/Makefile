CC = clang
AS = nasm
LD = ld.lld

OBJDIR = ../obj
STDLIB = ../stdlib

CFLAGS = --target=i386-pc-none-elf  -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -Wall -Wextra -nostdlib
LDFLAGS = -nostdlib -static -z max-page-size=0x1000 --gc-sections -T link.ld

OBJS = $(OBJDIR)/entry.o $(OBJDIR)/interrupts.o $(OBJDIR)/main.o $(OBJDIR)/serial.o $(OBJDIR)/idt.o $(OBJDIR)/shell.o $(OBJDIR)/gdt.o \
	$(OBJDIR)/bitmap.o $(OBJDIR)/kalloc.o $(OBJDIR)/paging.o $(OBJDIR)/sched.o  $(OBJDIR)/ps2_generic.o $(OBJDIR)/ps2_keyboard.o $(OBJDIR)/ps2_mouse.o \
	$(OBJDIR)/syscall.o $(OBJDIR)/gui-main.o $(OBJDIR)/floppy_asm.o $(OBJDIR)/floppy.o $(OBJDIR)/fat12.o $(OBJDIR)/apm_asm.o \
	$(OBJDIR)/apm.o $(OBJDIR)/fs_api.o $(OBJDIR)/vga.o $(OBJDIR)/exec.o $(OBJDIR)/svga.o
STDLIBS = $(STDLIB)/stdmem.o $(STDLIB)/string.o

all: kernel.elf

kernel.elf: $(OBJS) $(STDLIBS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(STDLIBS)

$(OBJDIR)/entry.o: entry.asm
	$(AS) -f elf32 entry.asm -o $@

$(OBJDIR)/interrupts.o: interrupts.asm
	$(AS) -f elf32 interrupts.asm -o $@

$(OBJDIR)/main.o: main.c ../include/serial/serial.h
	$(CC) $(CFLAGS) -c main.c -o $@

$(OBJDIR)/serial.o: ../include/serial/serial.c ../include/serial/serial.h
	$(CC) $(CFLAGS) -c ../include/serial/serial.c -o $@

$(OBJDIR)/bitmap.o: page/bitmap.c
	$(CC) $(CFLAGS) -c page/bitmap.c -o $@

$(OBJDIR)/idt.o: idt.c
	$(CC) $(CFLAGS) -c idt.c -o $@

$(OBJDIR)/shell.o: shell.c
	$(CC) $(CFLAGS) -c shell.c -o $@

$(OBJDIR)/gdt.o: gdt.c
	$(CC) $(CFLAGS) -c gdt.c -o $@

$(OBJDIR)/kalloc.o: page/kalloc.c
	$(CC) $(CFLAGS) -c page/kalloc.c -o $@

$(OBJDIR)/paging.o: page/paging.c page/paging.h
	$(CC) $(CFLAGS) -c page/paging.c -o $@

$(OBJDIR)/sched.o: sched/sched.c
	$(CC) $(CFLAGS) -c sched/sched.c -o $@

$(OBJDIR)/ps2_generic.o: driver/ps2/generic.c
	$(CC) $(CFLAGS) -c driver/ps2/generic.c -o $@

$(OBJDIR)/ps2_keyboard.o: driver/ps2/keyboard.c
	$(CC) $(CFLAGS) -c driver/ps2/keyboard.c -o $@

$(OBJDIR)/ps2_mouse.o: driver/ps2/mouse.c
	$(CC) $(CFLAGS) -c driver/ps2/mouse.c -o $@

$(OBJDIR)/syscall.o: syscall.c
	$(CC) $(CFLAGS) -c syscall.c -o $@

$(OBJDIR)/gui-main.o: gui/main.c
	$(CC) $(CFLAGS) -c gui/main.c -o $@

$(OBJDIR)/gui-font.o: gui/font/font.c
	$(CC) $(CFLAGS) -c gui/font/font.c -o $@

$(OBJDIR)/gui-font-render.o: gui/font/render.c
	$(CC) $(CFLAGS) -c gui/font/render.c -o $@

$(OBJDIR)/floppy_asm.o: driver/floppy/floppy.asm
	$(AS) -f elf32 driver/floppy/floppy.asm -o $@

$(OBJDIR)/floppy.o: driver/floppy/floppy.c
	$(CC) $(CFLAGS) -c driver/floppy/floppy.c -o $@

$(OBJDIR)/fat12.o: fs/fat12.c
	$(CC) $(CFLAGS) -c fs/fat12.c -o $@

$(OBJDIR)/apm_asm.o: driver/apm/apm.asm driver/apm/apm16.bin
	$(AS) -f elf32 driver/apm/apm.asm -o $@

$(OBJDIR)/apm.o: driver/apm/apm.c
	$(CC) $(CFLAGS) -c driver/apm/apm.c -o $@

driver/apm/apm16.bin: driver/apm/apm16.asm
	$(AS) -f bin driver/apm/apm16.asm -o $@

$(OBJDIR)/fs_api.o: fs/api.c
	$(CC) $(CFLAGS) -c fs/api.c -o $@

$(OBJDIR)/vga.o: driver/vga/vga.c
	$(CC) $(CFLAGS) -c driver/vga/vga.c -o $@

$(OBJDIR)/exec.o: sched/exec.c
	$(CC) $(CFLAGS) -c sched/exec.c -o $@

$(OBJDIR)/svga.o: driver/svga/svga.c
	$(CC) $(CFLAGS) -c driver/svga/svga.c -o $@

.PHONY: all
