OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS {
    . = 0x10000;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata


    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    KernelBssStart = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data
    KernelBssEnd = .;

    KernelEnd = .;

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }

}
