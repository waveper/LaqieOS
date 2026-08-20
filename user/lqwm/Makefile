CC = clang
AS = nasm
LD = ld.lld

CFLAGS = --target=i486-pc-none-elf  -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -Wall -Wextra -nostdlib
LDFLAGS = -nostdlib -static -z max-page-size=0x1000 --gc-sections -T ../link.ld

all: lqwm.elf
	llvm-strip lqwm.elf -o lqwm.self
	llvm-objcopy -O binary lqwm.self lqwm.bin
	rm lqwm.self

fb-req.o: fb-req.asm
	$(AS) -f elf32 fb-req.asm -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

lqwm.elf: main.o fb-req.o
	$(LD) $(LDFLAGS) ../crt.o main.o fb-req.o  -o lqwm.elf

.PHONY: all clean

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.self
