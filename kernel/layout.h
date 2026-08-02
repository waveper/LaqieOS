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

#endif
