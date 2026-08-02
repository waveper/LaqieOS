org 0x7C00
bits 16

; The first sector of our disk will contain a header signalling that it 
; contains a FAT12 file system, as well as the boot loader that will load the
; rest of the OS. 
;
; When the BIOS recognizes our floppy disk as bootable using the boot sector
; signature, it will load the entire boot sector into RAM at location
; 0000:7C00, and start executing it immediately. 
;
; Since the BIOS doesn't care about our FAT header, we need to jump over it's
; data to prevent the CPU from trying to interpret the header as executable 
; code. The FAT specification is aware of this, and actually enforces that the
; first 3 bytes of the boot sector be a short jump to the start of the boot code. 
; 
; After jumping over the header to our bootloader code, we will load the stage
; 2 bootloader from the disk.
;
; Refences:
; - https://dev.to/frosnerd/writing-my-own-boot-loader-3mld
; - https://www.youtube.com/@olivestemlearning
;

; FAT Boot Sector Header
; https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#Boot_Sector

; Short Jump over the header
jmp short main
nop

; FAT12 OEM Name
fat12_oem:                  db 'MSWIN4.1'

; BIOS Parameter Block
; https://en.wikipedia.org/wiki/BIOS_parameter_block#DOS_3.31_BPB
bpb_bytes_per_sector:       dw 512
bpb_sectors_per_cluster:    db 1
bpb_reserved_sectors:       dw 1
bpb_fat_count:              db 2
bpb_dir_entries_count:      dw 0x0E0
bpb_total_sectors:          dw 2880
bpb_media_descriptor_type:  db 0xF0
bpb_sectors_per_fat:        dw 9
bpb_sectors_per_track:      dw 18
bpb_heads:                  dw 2
bpb_hidden_sectors:         dd 0
bpb_large_sector_count:     dd 0

; Extended BIOS Parameter Block
; https://en.wikipedia.org/wiki/BIOS_parameter_block#DOS_4.0_EBPB
ebpb_drive_number:   db 0
ebpb_flags:          db 0
ebpb_signature:      db 0x29                   ; 4.1
ebpb_volume_id:      db 0x12, 0x34, 0x56, 0x78 ; Volume serial number
ebpb_volume_label:   db 'LAQIEOS    ' 
ebpb_file_system_id: db 'FAT12   '

;
; Beginning of bootloader code
;
; Before the BIOS moves execution to the bootloader, it sets dl to the number
; of the physical drive that was booted from.
;
; Bootloader Memory Layout:
; 0x1000-4DFF - Second stage bootloader (15KiB)
; 0x4E00-69FF - FAT Root Directory (14 * 512 = 7KiB)
; 0x6A00-7BFF - FAT Table (9 * 512 = 4.5KiB)
; 0x7C00-7DFF - MBR Loaded by the BIOS bootsector-loader (512B)
; 0x7E00-7FFF - Bootloader Stack (512B)
; 0x8000-FFFF - Kernel Stack (32KiB)
; 0x10000-1FFFF - Kernel (64KiB)
;
; x86 memory layout reference: https://i.stack.imgur.com/A8gMs.png
;
main:
    ; Initialize the registers of the processor to a known state
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Setup the stack to a known place in memory (grows down)
    mov bp, BOOTLOADER_STACK_BASE
    mov sp, bp

    ; Store the booted drive number (from BIOS) into memory
    mov [ebpb_drive_number], dl

    ; Read FAT table from disk
    .load_fat_table:
        mov ax, 1                   ; Sector 1 on the disk
        mov bx, FAT_TABLE_ADDR      ; Load into memory at this addr
        mov cl, 9                   ; Read 9 sectors
        call disk_read

    ; Read FAT root dir from disk
    .load_root_dir:
        mov ax, 19                  ; Sector 19 on the disk
        mov bx, FAT_ROOT_DIR_ADDR   ; Load into memory at this addr
        mov cl, 14                  ; Read 14 sectors
        call disk_read

    ; Load the second stage bootloader into memory
    .load_stage2:
        mov si, stage2_file_name
        mov bx, STAGE2_ADDR
        call fat_find_and_read_root_file

    ; Pass pointers to bootloader functions to stage 2
    .load_fn_pointers:
        mov ax, print
        mov bx, fat_find_and_read_root_file

    ; Jump to the second stage bootloader that we just loaded into memory
    .jump_to_stage2:
        jmp STAGE2_ADDR

halt:
    ; Halt the processor
    hlt
    jmp halt

; Address Constants
BOOTLOADER_STACK_BASE equ 0x8000
STAGE2_ADDR equ 0x1000
FAT_TABLE_ADDR equ 0x6A00
FAT_ROOT_DIR_ADDR equ 0x4E00
stage2_file_name: db 'BOOT    BIN', 0

;
; Disk operations that use the int 0x13 BIOS interrupt:
;
; https://en.wikipedia.org/wiki/INT_13H
;

;
; Converts a disk location from LBA indexing to CHS indexing format
;   @input ax - LBA index
;   @output cx [bits 0-5] - sector number
;   @output cx [bits 6-15] - cylinder
;   @output dh - head
;
lba_to_chs:
    push ax
    push dx

    ; Clear dx
    xor dx, dx

    ; LBA % sectors per track + 1 = sector
    div word [bpb_sectors_per_track] 
    inc dx 
    mov cx, dx ; Sector in cx

    xor dx, dx
    div word [bpb_heads] 

    ; (LBA / sectors per track) % number of heads = head
    mov dh, dl ; Head in dh

    ; (LBA / sectors per track) / number of heads  = cylinder
    mov ch, al
    shl ah, 6
    or cl, ah ; Cylinder in cx [bits 6-15]

    pop ax
    mov dl, al
    pop ax

    ret

;
; Reads the given sector(s) from a disk
; @input ax - LBA index to read from
; @input cl - Number of sectors to read
; @input dl - Drive number
; @input es:bx - Destination address
;
disk_read:
    pusha
    push di

    ; Make room for 4 bytes on the stack
    push bp
    mov bp, sp
    sub sp, 4

    ; Store the number of sectors to read on the stack
    mov [bp-4], cl

    ; Convert the LBA sector index to CHS
    call lba_to_chs

    ; Create a retry counter to tell us when to stop reading after failures
    mov di, 3 ; Counter

.try_read:
    ; Restore the number of sectors to read
    mov al, [bp-4]
    
    ; BIOS interrupt to read a sector
    stc
    mov ah, 0x02
    int 0x13

    ; If carry is set, there was an error
    jc .read_error

    ; If the number of sectors read and the number we requested are different,
    ; there was an error
    cmp [bp-4], al
    jne .read_error

    ; Jump to end if succeeded
    jmp .read_done

.read_error:
    ; If failed, reset the disk system
    call disk_reset

    ; If we havent reached the retry limit, try again
    dec di
    test di, di
    jnz .try_read

    ; If we have reached the limit, jump to hard failure
    jmp disk_fail

.read_done:
    ; Restore the stack
    mov sp, bp
    pop bp
    
    ; Restore all other registers
    pop di
    popa
    
    ret

;
; Resets the drivers for the given disk 
; @input dl - drive number
;
disk_reset:
    pusha

    ; BIOS interrupt to reset disk system
    mov ah, 0
    stc
    int 0x13

    ; If failed to reset, jump to hard failure
    jc disk_fail
    
    popa
    ret

;
; Prints an disk read error message and halts
;
disk_fail:
    ; Print a failure message and halt
    mov si, read_failure_msg
    call print_halt

read_failure_msg: db 'ERRDSK', 0

;
; Naive implementation of basic FAT12 driver
;
; https://www.eit.lth.se/fileadmin/eit/courses/eitn50/Literature/fat12_description.pdf
; https://www.sqlpassion.at/archive/2022/03/03/reading-files-from-a-fat12-partition/
;

; Algorithm to read and unpack the nth FAT entry from the table
; 
; - If n is even, then the physical location of the entry is the low four bits in location 1+(3*n)/2
;   and the 8 bits in location (3*n)/2
; - If n is odd, then the physical location of the entry is the high four bits in location (3*n)/2 and
;   the 8 bits in location 1+(3*n)/2
;
; If we read 2 bytes from the FAT at the index 
;
; if (n % 2 == 0) {
;   short entry = FAT[(3 * n) / 2] & 0x0FFF
; } else {
;   short entry = FAT[(3 * n) / 2] >> 4
; }
;
; @input ax - FAT index
; @output ax - FAT entry
fat_read_entry_from_fat:
    push cx
    push bx
    
    ; Store FAT index for later use
    push ax

    ; Calculate start address offset of 12-bit entry
    mov cx, 3
    mul cx      ; n * 3
    shr ax, 1   ; (n * 3) / 2

    ; Add offset to calculate start address of entry
    mov bx, FAT_TABLE_ADDR
    add bx, ax

    ; Read the entry bytes into cx
    mov cx, word [bx]

    ; Branch based on parity of index (determines unpacking strategy)
    pop ax
    test ax, 1 ; Sets zero bit to LSB of index (1 if odd 0 is even)
    jnz .odd

.even:
    and cx, 0x0FFF

    jmp .read_done

.odd:
    shr cx, 4
    
.read_done:
    ; Move entry back into ax for return
    mov ax, cx
    
    pop bx
    pop cx
    ret

;
; Reads an entire file from the disk into memory given the index of the first cluster
; 
; @input ax - First logical cluster index
; @input es:cx - Destination address
;
fat_read_file_from_fat:
    pusha

    ; Make room for 4 bytes on the stack
    mov bp, sp
    sub sp, 4

    ; Store input variables on the stack
    mov [bp-2], ax      ; Current cluster index
    mov [bp-4], cx      ; Dest base pointer

    ; Create an incrementing sector offset to add to the dest base pointer
    mov di, 0

.fat_loop:
    mov ax, [bp-2] ; Current cluster index

.disk_read:
    ; Converts the FAT index into a physical sector number on the disk
    add ax, 33
    sub ax, 2 ; Sector number in ax

    ; Calculate the dest address from the offset
    mov bx, di
    shl bx, 9       ; Multiply by 512
    mov cx, [bp-4]  ; Dest base pointer
    add bx, cx      ; Read dest in bx
    
    ; Read the sector into memory
    mov cl, 1                   ; Sectors to read
    mov dl, [ebpb_drive_number] ; Drive number to read from
    call disk_read

    ; Next iteration, write 512 bytes further into memory
    inc di

.get_entry:
    ; Get entry from FAT at the current index
    mov ax, [bp-2]                  ; Current cluster index
    call fat_read_entry_from_fat    ; FAT[curr_idx] in ax
    mov [bp-2], ax                  ; curr_idx = FAT[curr_idx]

    ; If the next entry is a valid index, keep traversing, otherwise we are finished reading
    ; 
    ; 0x000: Unused
    ; 0x001: Reserved Cluster
    ; 0x002 – 0xFEF: The cluster is in use, and the value represents the next cluster
    ; 0xFF0 – 0xFF6: Reserved Cluster
    ; 0xFF7: Bad Cluster
    ; 0xFF8 – 0xFFF: Last Cluster in a file
    ;
    ; We can tell if we should keep reading if (entry - 2) <= 0xFED
    ; We can tell that we have reached the end of the file if (entry >> 3) == 0x1FF

    ; Keep traversing case
    sub ax, 2
    cmp ax, 0xFED
    jle .fat_loop

    ; EOF case
    mov ax, [bp-2]
    shr ax, 3
    cmp ax, 0x1FF
    je .read_done

.error:
    ; Print a failure message and halt
    mov si, fat_read_entry_failure_msg
    call print_halt

.read_done:
    add sp, 4

    popa
    ret

;
; Checks to see if the file name of this entry matches a given string
; @input si - file name
; @input es:bx - pointer to the directory entry
;
fat_dir_entry_matches:
    pusha

    ; Store pointer for later
    mov cx, bx

    ; Initialize a counter
    mov di, 0

.match_loop:
    ; Load a byte from the file name into al
    lodsb

    ; Compare with the same byte from the entry file name
    mov bx, cx
    add bx, di
    mov ah, [bx]
    cmp al, ah
    jne .not_matched

    ; If the counter reached 11 without failing early, then the strings match
    inc di
    cmp di, 11
    je .matched

    ; Continue the loop
    jmp .match_loop

.matched:
    ; Set zero flag
    lahf                      ; Load AH from FLAGS
    or       ah, 001000000b    ; Set bit for ZF
    sahf                      ; Store AH back to Flags

    jmp .finished

.not_matched:
    ; Clear zero flag
    lahf                      ; Load lower 8 bit from Flags into AH
    and      ah, 010111111b    ; Clear bit for ZF
    sahf                      ; Store AH back to Flags

    jmp .finished

.finished:
    popa
    ret

;
; Searches for a reads a file from the root directory into memory 
; @input si - File name
; @input es:bx - Destination address
;
fat_find_and_read_root_file:
    pusha

    ; Store the destination addr for later
    mov cx, bx

    ; Entry index (32-bits each)
    mov di, 0

.search_loop:
    ; Calculate the next entry address
    mov bx, di
    shl bx, 5
    add bx, FAT_ROOT_DIR_ADDR ; entry addr in bx
    
    ; Check if the file name matches the given one
    call fat_dir_entry_matches
    je .file_found

    ; Next time around, get the entry after this one
    inc di

    ; If we are at the end of the root directory, we didnt find the file
    cmp di, 224
    je .file_not_found

    ; Continue the loop
    jmp .search_loop

.file_not_found:
    ; Print error code and space
    mov si, fat_file_not_found_msg
    call print

    ; Print file name and halt
    popa
    call print_halt

.file_found:
    ; Get the number of the first logical cluster
    add bx, 26 ; Offset into directory entry
    mov ax, [bx]

    ; Read the entire file into memory
    call fat_read_file_from_fat

.search_done:
    popa
    ret

fat_read_entry_failure_msg: db 'ERRFAT', 0
fat_file_not_found_msg: db 'ERRFNF: ', 0

; 
; Prints a given string to the screen using BIOS interrupts
; @input si - Pointer to the string to print
;
print: 
    pusha

.print_loop:
    ; Load a character from si register into al 
    lodsb 

    ; If the character is null (the end of the string) jump to the end of the routine
    or al, al
    jz .print_done

    ; BIOS interrupt to print char
    mov ah, 0x0E
    mov bh, 0
    int 0x10

    ; Continue the loop
    jmp .print_loop

.print_done:
    popa
    ret

print_halt:
    call print
    jmp halt

times 510 - ($ - $$) db 0
dw 0xAA55
