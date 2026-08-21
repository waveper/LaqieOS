CC = clang
AS = nasm
LD = ld.lld

CFLAGS = --target=i486-pc-none-elf  -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -Wall -Wextra -nostdlib
LDFLAGS = -nostdlib -static -z max-page-size=0x1000 --gc-sections -T link.ld

all: hello.bin example.bin segfault.bin string.o stdlib.o shell.bin illegal.bin ta.bin
	$(MAKE) -C lqwm

crt.o: crt.asm
	$(AS) -f elf32 crt.asm -o $@

illegal.bin: illegal.asm
	$(AS) -f bin illegal.asm -o $@

hello.o: hello.c
	$(CC) $(CFLAGS) -c hello.c -o hello.o

example.o: example.c
	$(CC) $(CFLAGS) -c example.c -o example.o

hello.elf: crt.o hello.o link.ld
	$(LD) $(LDFLAGS) crt.o hello.o -o hello.elf

example.elf: crt.o example.o link.ld
	$(LD) $(LDFLAGS) crt.o example.o -o example.elf

hello.bin: hello.elf
	llvm-strip hello.elf -o hello.self
	llvm-objcopy -O binary hello.self hello.bin
	rm hello.self

example.bin: example.elf
	llvm-strip example.elf -o example.self
	llvm-objcopy -O binary example.self example.bin
	rm example.self

segfault.o: segfault.c
	$(CC) $(CFLAGS) -c segfault.c -o segfault.o

segfault.elf: crt.o segfault.o link.ld
	$(LD) $(LDFLAGS) crt.o segfault.o -o segfault.elf

segfault.bin: segfault.elf
	llvm-strip segfault.elf -o segfault.self
	llvm-objcopy -O binary segfault.self segfault.bin
	rm segfault.self

string.o: string.c string.h
	$(CC) $(CFLAGS) -c string.c -o string.o

stdlib.o: stdlib.c stdlib.h
	$(CC) $(CFLAGS) -c stdlib.c -o stdlib.o

shell.o: shell.c stdio.h string.h
	$(CC) $(CFLAGS) -c shell.c -o shell.o

shell.elf: crt.o shell.o string.o link.ld
	$(LD) $(LDFLAGS) crt.o shell.o string.o -o shell.elf

shell.bin: shell.elf
	llvm-strip shell.elf -o shell.self
	llvm-objcopy -O binary shell.self shell.bin
	rm shell.self

ta.o: test-alloc.c
	$(CC) $(CFLAGS) -c test-alloc.c -o ta.o

ta.elf: ta.o
	$(LD) $(LDFLAGS) crt.o ta.o stdlib.o string.o -o ta.elf

ta.bin: ta.elf
	llvm-strip ta.elf -o ta.self
	llvm-objcopy -O binary ta.self ta.bin
	rm ta.self

.PHONY: all clean

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.self
	$(MAKE) -C lqwm clean
