IMG = LaqieOS.img
KERNEL_ELF = kernel/kernel.elf
KERNEL_BIN = kernel.bin
BOOTSECTOR = boot/bootsector.bin
BOOTBIN = boot/boot.bin

all: $(IMG)

compile_kernel:
	mkdir -p obj
	$(MAKE) -C stdlib
	$(MAKE) -C kernel

compile_boot:
	$(MAKE) -C boot

compile_user:
	$(MAKE) -C user

$(KERNEL_BIN): compile_kernel
	llvm-strip kernel/kernel.elf -o kernel.self
	llvm-objcopy -O binary kernel.self $(KERNEL_BIN)
	rm kernel.self

$(IMG): compile_boot $(KERNEL_BIN) compile_user
	dd if=/dev/zero of=$(IMG) bs=512 count=2880
	mkfs.vfat -F 12 -n "LAQIEOS" $(IMG)
	dd if=boot/bootsector.bin of=$(IMG) conv=notrunc bs=512 count=1
	mcopy -i $(IMG) boot/boot.bin ::/
	mcopy -i $(IMG) $(KERNEL_BIN) ::/
	mcopy -i $(IMG) user/hello.bin ::/
	mcopy -i $(IMG) user/lqwm/lqwm.bin ::/
	mcopy -i $(IMG) user/example.bin ::/
	mcopy -i $(IMG) user/segfault.bin ::/
	mcopy -i $(IMG) user/shell.bin ::/
	mcopy -i $(IMG) user/illegal.bin ::/

.PHONY: all clean run run-nogui run-log compile_kernel compile_boot compile_user

run:
	qemu-system-i386 -m 32M -fda $(IMG) -boot a -vga std -serial mon:stdio

run-nogui:
	qemu-system-i386 -m 8M -fda $(IMG) -boot a -vga std -serial mon:stdio -nographic

run-log:
	qemu-system-i386 -m 8M -fda $(IMG) -boot a -vga std -serial file:serial.log -nographic

clean:
	rm -f $(IMG)
	rm -rf obj
	rm -f kernel/kernel.elf
	rm -f kernel.bin
	rm -f boot/bootsector.bin
	rm -f boot/boot.bin
	rm -f kernel/driver/apm/apm16.bin
	rm -f serial.log
	$(MAKE) -C user clean
	$(MAKE) -C stdlib clean
