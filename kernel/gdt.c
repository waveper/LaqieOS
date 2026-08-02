#include <stdint.h>
#include <stdbool.h>
#include "../stdlib/stdmem.h"
#include "layout.h"
#define GDT_SIZE 6

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR   0x18
#define USER_DATA_SELECTOR   0x20
#define TSS_SELECTOR         0x28

struct tss_entry
{
        uint32_t prev_tss;   // Previous TSS link
        uint32_t esp0;       // Kernel stack pointer
        uint32_t ss0;        // Kernel stack segment
        uint32_t esp1;       // Unused
        uint32_t ss1;        // Unused
        uint32_t esp2;       // Unused
        uint32_t ss2;        // Unused
        uint32_t cr3;        // Unused
        uint32_t eip;        // Unused
        uint32_t eflags;     // Unused
        uint32_t eax;        // Unused
        uint32_t ecx;        // Unused
        uint32_t edx;        // Unused
        uint32_t ebx;        // Unused
        uint32_t esp;        // Unused
        uint32_t ebp;        // Unused
        uint32_t esi;        // Unused
        uint32_t edi;        // Unused
        uint32_t es;         // Unused
        uint32_t cs;         // Unused
        uint32_t ss;         // Unused
        uint32_t ds;         // Unused
        uint32_t fs;         // Unused
        uint32_t gs;         // Unused
        uint32_t ldt;        // Unused
        uint16_t trap;       // Unused
        uint16_t iomap_base; // I/O Map Base Address
} __attribute__((packed));

struct gdt_entry
{
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_middle;
        uint8_t access;
        uint8_t granularity;
        uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr
{
        uint16_t limit;
        uint32_t base;
} __attribute__((packed));

struct gdt_entry gdt[GDT_SIZE];
struct gdt_ptr gdtp;

void SetGDTEntry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        gdt[num].base_low = (base & 0xFFFF);
        gdt[num].base_middle = (base >> 16) & 0xFF;
        gdt[num].base_high = (base >> 24) & 0xFF;
        gdt[num].limit_low = (limit & 0xFFFF);
        gdt[num].granularity = (limit >> 16) & 0x0F;
        gdt[num].granularity |= (gran & 0xF0);
        gdt[num].access = access;
}

void SetTSSEntry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        gdt[num].base_low = (base & 0xFFFF);
        gdt[num].base_middle = (base >> 16) & 0xFF;
        gdt[num].base_high = (base >> 24) & 0xFF;
        gdt[num].limit_low = (limit & 0xFFFF);
        gdt[num].granularity = (limit >> 16) & 0x0F;
        gdt[num].granularity |= (gran & 0xF0);
        gdt[num].access = access;
}

struct tss_entry tss;

static void TSSInit(void) {
        memset(&tss, 0, sizeof(tss));
        tss.esp0 = KERNEL_STACK_TOP;
        tss.ss0 = KERNEL_DATA_SELECTOR;
        tss.iomap_base = sizeof(tss);
}

void TSSSetKernelStack(uint32_t esp0) {
        tss.esp0 = esp0;
}

void GDTInit(void) {
        gdtp.limit = (sizeof(struct gdt_entry) * GDT_SIZE) - 1;
        gdtp.base = (uint32_t)&gdt;
        TSSInit();
        SetGDTEntry(0, 0, 0, 0, 0);
        SetGDTEntry(1, 0, 0x000FFFFF, 0x9A, 0xCF);
        SetGDTEntry(2, 0, 0x000FFFFF, 0x92, 0xCF);
        SetGDTEntry(3, 0, 0x000FFFFF, 0xFA, 0xCF);
        SetGDTEntry(4, 0, 0x000FFFFF, 0xF2, 0xCF);
        SetTSSEntry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x40);
        asm volatile("lgdt (%0)" : : "r"(&gdtp));
        asm volatile("ltr %w0" : : "r"((uint16_t)TSS_SELECTOR));
        asm volatile("movl $0x10, %%eax; \
                      movl %%eax, %%ds; \
                      movl %%eax, %%es; \
                      movl %%eax, %%fs; \
                      movl %%eax, %%gs; \
                      movl %%eax, %%ss; \
                      ljmp $0x08, $goto; \
                      goto:" : : : "memory");
}
