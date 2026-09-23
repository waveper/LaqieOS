CC = clang
AS = nasm
LD = ld.lld

CFLAGS = --target=i486-pc-none-elf  -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -Wall -Wextra -nostdlib
LDFLAGS = -nostdlib -static -z max-page-size=0x1000 --gc-sections -T link.ld

all: string.o stdlib.o shell.bin loop.bin
	$(MAKE) -C lqwm

crt.o: crt.asm
	$(AS) -f elf32 crt.asm -o $@

loop.bin: loop.asm
	$(AS) -f bin loop.asm -o $@

string.o: string.c string.h
	$(CC) $(CFLAGS) -c string.c -o string.o

stdlib.o: stdlib.c stdlib.h
	$(CC) $(CFLAGS) -c stdlib.c -o stdlib.o

shell.o: shell.c stdio.h string.h
	$(CC) $(CFLAGS) -c shell.c -o shell.o

shell.elf: crt.o shell.o string.o stdlib.o link.ld
	$(LD) $(LDFLAGS) crt.o shell.o string.o stdlib.o -o shell.elf

shell.bin: shell.elf
	llvm-strip shell.elf -o shell.self
	llvm-objcopy -O binary shell.self shell.bin
	rm shell.self

.PHONY: all clean

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.self
	$(MAKE) -C lqwm clean
