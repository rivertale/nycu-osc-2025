.macro save_registers_to_stack
    sub sp, sp, 34 * 8
    stp x0, x1, [sp, 0 * 8]
    stp x2, x3, [sp, 2 * 8]
    stp x4, x5, [sp, 4 * 8]
    stp x6, x7, [sp, 6 * 8]
    stp x8, x9, [sp, 8 * 8]
    stp x10, x11, [sp, 10 * 8]
    stp x12, x13, [sp, 12 * 8]
    stp x14, x15, [sp, 14 * 8]
    stp x16, x17, [sp, 16 * 8]
    stp x18, x19, [sp, 18 * 8]
    stp x20, x21, [sp, 20 * 8]
    stp x22, x23, [sp, 22 * 8]
    stp x24, x25, [sp, 24 * 8]
    stp x26, x27, [sp, 26 * 8]
    stp x28, x29, [sp, 28 * 8]
    str x30, [sp, 30 * 8]
    mrs x0, spsr_el1
    mrs x1, elr_el1
    stp x0, x1, [sp, 32 * 8]
.endm

.macro load_registers_from_stack
    ldp x0, x1, [sp, 32 * 8]
    msr spsr_el1, x0
    msr elr_el1, x1
    ldp x0, x1, [sp, 0 * 8]
    ldp x2, x3, [sp, 2 * 8]
    ldp x4, x5, [sp, 4 * 8]
    ldp x6, x7, [sp, 6 * 8]
    ldp x8, x9, [sp, 8 * 8]
    ldp x10, x11, [sp, 10 * 8]
    ldp x12, x13, [sp, 12 * 8]
    ldp x14, x15, [sp, 14 * 8]
    ldp x16, x17, [sp, 16 * 8]
    ldp x18, x19, [sp, 18 * 8]
    ldp x20, x21, [sp, 20 * 8]
    ldp x22, x23, [sp, 22 * 8]
    ldp x24, x25, [sp, 24 * 8]
    ldp x26, x27, [sp, 26 * 8]
    ldp x28, x29, [sp, 28 * 8]
    ldr x30, [sp, 30 * 8]
    add sp, sp, 34 * 8
.endm

.section ".text"
.align 11
.global exception_vector_table
exception_vector_table:
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7

    b exception_entry_el1_cur_sync
    .align 7
    b exception_entry_el1_cur_irq
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7

    b exception_entry_el1_low_sync
    .align 7
    b exception_entry_el1_low_irq
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7

    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7
    b exception_entry_default
    .align 7

exception_entry_el1_cur_sync:
    save_registers_to_stack
    bl exception_handler_el1_cur_sync
    load_registers_from_stack
    eret

exception_entry_el1_cur_irq:
    save_registers_to_stack
    bl exception_handler_el1_cur_irq
    load_registers_from_stack
    eret

exception_entry_el1_low_sync:
    save_registers_to_stack
    bl exception_handler_el1_low_sync
    load_registers_from_stack
    eret

exception_entry_el1_low_irq:
    save_registers_to_stack
    bl exception_handler_el1_low_irq
    load_registers_from_stack
    eret

exception_entry_default:
    save_registers_to_stack
    bl exception_handler_default
    load_registers_from_stack
    eret
