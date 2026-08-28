#ifndef KERNEL_LAYOUT_H
#define KERNEL_LAYOUT_H

// Reserved top of the bootstrap kernel stack region.
#define KERNEL_STACK_TOP 0x90000

/*
 * User programs run at stable virtual addresses. Each process maps private
 * physical pages behind these ranges.
 */
#define USER_EXEC_LOAD_ADDR 0x100000
#define USER_EXEC_MAX_SIZE  0x10000
#define USER_EXEC_END       (USER_EXEC_LOAD_ADDR + USER_EXEC_MAX_SIZE)
#define USER_STACK_SIZE     0x2000
#define USER_STACK_TOP      0x120000
#define USER_STACK_BASE     (USER_STACK_TOP - USER_STACK_SIZE)
#define USER_SPACE_END      USER_STACK_TOP

/*
 * User heap: a per-task bump allocator region. Each allocation is backed by
 * fresh physical pages mapped here at dedicated user virtual addresses (never
 * the kernel's physical addresses), so a task can never reach kernel memory
 * through malloc/free. Lives in its own 4 MB page-directory slot (0x400000)
 * to avoid colliding with the low-memory exec/stack/shared mappings.
 */
#define USER_HEAP_BASE      0x400000
#define USER_HEAP_SIZE      0x400000
#define USER_HEAP_END       (USER_HEAP_BASE + USER_HEAP_SIZE)

#endif
