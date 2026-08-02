org 0x1000
bits 16

; The second stage of out bootloader is responsible for, loading the kernel 
; into memory, setting up the Global Descriptor Table (GDT), making the jump to
; 32-bit protected mode, and finally calling the kernel entry point.

BOOT_INFO_ADDR equ 0x0500
E820_BUFFER   equ 0x0600
FAT_ROOT_DIR_ADDR equ 0x4E00

main:
    ; Load function pointers from the boot sector
    .load_function_pointers:
        mov [print_fp], ax
        mov [fat_find_and_read_root_file_fp], bx


    ; Query the BIOS for the installed memory size before leaving real mode.
    .detect_memory:
        call detect_memory_size

    ; currently disabled
    .set_video_mode:
      call SetVideo

    ; Load the kernel from the disk using the FAT driver from the boot sector
    .load_kernel_from_disk:
        mov si, kernel_file_name
        call find_root_file_size
        test dx, dx
        jnz kernel_too_large

        mov si, kernel_file_name
        mov ax, KERNEL_ADDR_SEGMENT
        mov es, ax
        mov bx, KERNEL_ADDR_OFFSET
        call [fat_find_and_read_root_file_fp]

        mov si, kernel_load_msg
        call [print_fp]

    ; Switch over the CPU into 32-bit protected mode with flat memory
    ; addressing (No page table just yet)
    .switch_to_32_bits:
        call enable_a20

        ; Disable all CPU interrupts
        cli
        
        ; Load our GDT
        lgdt [GDT_descriptor]
        
        ; Switch to protected mode
        mov eax, cr0
        or eax, 1
        mov cr0, eax

        ; Reload the segment registers
        call gdt_reload_segments

    [bits 32]
    ; Set up kernel stack
    .create_kernel_stack:
        mov ebp, KERNEL_STACK_ADDR
        mov esp, ebp

    ; Call the kernel entry point
    .jump_to_kernel:
        jmp KERNEL_ADDR

; Address Constants
KERNEL_STACK_ADDR equ 0x90000
KERNEL_ADDR_SEGMENT equ 0x1000
KERNEL_ADDR_OFFSET equ 0x0000
KERNEL_ADDR equ KERNEL_ADDR_SEGMENT * 0x10 + KERNEL_ADDR_OFFSET

%include "stage2/gdt.asm"
%include "stage2/vbe.asm"

; Enable the A20 line so 32-bit code can safely access memory above 1 MiB.
enable_a20:
    in al, 0x92
    test al, 0x02
    jnz .done
    or al, 0x02
    and al, 0xFE
    out 0x92, al
.done:
    ret

find_root_file_size:
    mov di, FAT_ROOT_DIR_ADDR
    mov cx, 224

.next_entry:
    push si
    push di
    mov bx, 11

.match_loop:
    lodsb
    cmp al, [di]
    jne .not_found_here
    inc di
    dec bx
    jnz .match_loop

    pop di
    pop si
    mov ax, [di + 28]
    mov dx, [di + 30]
    ret

.not_found_here:
    pop di
    pop si
    add di, 32
    loop .next_entry

    xor ax, ax
    xor dx, dx
    ret

; Store the number of installed bytes above 1 MiB at BOOT_INFO_ADDR.
; Uses INT 15h, EAX=E820h when available and falls back to E801h/88h.
detect_memory_size:
    call detect_memory_e820
    test eax, eax
    jnz .store

    call detect_memory_legacy
.store:
    mov dword [BOOT_INFO_ADDR], eax
    ret

detect_memory_e820:
    xor esi, esi
    xor ebx, ebx
    xor ax, ax
    mov es, ax

.loop:
    mov di, E820_BUFFER
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [es:di + 20], 1
    int 0x15
    jc .fail
    cmp eax, 0x534D4150
    jne .fail

    cmp dword [es:di + 16], 1
    jne .next
    cmp dword [es:di + 12], 0
    jne .next
    cmp dword [es:di + 4], 0
    jne .next

    mov eax, [es:di + 8]
    mov edx, [es:di + 0]
    cmp edx, 0x00100000
    jae .add_full

    add edx, eax
    cmp edx, 0x00100000
    jbe .next
    sub edx, 0x00100000
    add esi, edx
    jmp .next

.add_full:
    add esi, eax

.next:
    test ebx, ebx
    jnz .loop

    mov eax, esi
    ret

.fail:
    xor eax, eax
    ret

detect_memory_legacy:
    mov ax, 0xE801
    int 0x15
    jc .fallback_88

    ; Some BIOSes only return AX/BX, others CX/DX.
    test ax, ax
    jnz .have_e801
    test bx, bx
    jnz .have_e801
    mov ax, cx
    mov bx, dx

.have_e801:
    xor eax, eax
    movzx ecx, ax
    shl ecx, 10
    add eax, ecx

    movzx ecx, bx
    shl ecx, 16
    add eax, ecx
    ret

.fallback_88:
    mov ah, 0x88
    int 0x15
    jc .fail_legacy

    xor eax, eax
    movzx ecx, ax
    shl ecx, 10
    mov eax, ecx

    ret

.fail_legacy:
    xor eax, eax
    ret

kernel_too_large:
    mov si, kernel_too_large_msg
    call [print_fp]
    cli
.halt:
    hlt
    jmp .halt

; Pointers to functions in the boot sector (used to remove code duplication)
print_fp: dw 0
fat_find_and_read_root_file_fp: dw 0

kernel_load_msg: db 'Entering Kernel...', 0x0D, 0x0A, 0
kernel_too_large_msg: db 'Kernel image exceeds 64KiB loader limit', 0x0D, 0x0A, 0
kernel_file_name: db 'KERNEL  BIN', 0
