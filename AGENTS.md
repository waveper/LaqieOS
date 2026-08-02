# Project Information

This is LaqieOS, non-posix compatible OS, also the OS only target i386 architechture

# Directory Structure
`boot/` contain the two-stage bootloader written in assembly only NASM syntax
`kernel/` holds the freestanding i386 kernel, including core startup (`main.c`, `entry.asm`), interrupts, scheduling, RAM FS, GUI, paging, and device drivers under `driver/`
and small libc-style helpers live in `stdlib/`.
Build artifacts are written to `obj/`

## Build, Test, and Development Commands
Use the top-level `Makefile`.

- `make` builds `LaqieOS.img`, including bootloader, kernel ELF, stripped kernel binary
- `make run` boots the image in QEMU with VGA output and serial console on stdio.
- `make run-nogui` runs headless QEMU for quick serial-only debugging.
- `make run-log` writes serial output to `serial.log` for postmortem inspection.
- `make clean` removes generated images, dumps, binaries, and delegated sub-build outputs.

This project expects `clang`, `ld.lld`, `llvm-objcopy`, `llvm-strip`, `llvm-objdump`, `nasm`, QEMU, and FAT utilities such as `mkfs.vfat` and `mcopy`.

## Coding Style & Naming Conventions
Follow the existing local style rather than introducing a new formatter. C code generally uses 2-space indentation with K&R-style function bodies in newer files; keep includes explicit and avoid libc dependencies beyond the local `stdlib/`. Use `PascalCase` for kernel entry points and subsystem APIs such as `KMain`, `PITInit`, and `AppendTaskRing0`. Use `snake_case` or descriptive lowercase labels in assembly. Keep file names lowercase and grouped by subsystem

## Testing Guidelines
There is no standalone unit-test framework yet. Treat a clean `make` plus a successful QEMU boot as the minimum validation bar. For kernel changes, verify the serial log or interactive shell path affected by the edit. For bootloader changes, confirm the image still boots from `make run` and that stage 2 loads `kernel.bin`.

## Commit & Pull Request Guidelines
Recent history favors short, imperative commit subjects such as `Fixed PS2 Keyboard and Mouse IRQ` and `Implemented PS2 Mouse Driver and VGA Graphics`. Keep subjects specific to one subsystem and under roughly 72 characters. Pull requests should describe the runtime impact, list the commands used to validate the change, and include serial output or screenshots when the change affects boot flow, VGA, input, or shell behavior.
