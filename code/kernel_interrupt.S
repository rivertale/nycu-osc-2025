.section ".text"

// NOTE: the layout must be exactly the same as TrapFrame
.macro save_trap_frame
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
    stp x28, fp, [sp, 28 * 8]
    mrs x0, spsr_el1
    mrs x1, elr_el1
    mrs x2, sp_el0
    stp lr, x0, [sp, 30 * 8]
    stp x1, x2, [sp, 32 * 8]
.endm

// NOTE: the layout must be exactly the same as TrapFrame
.macro load_trap_frame
    ldp lr, x0, [sp, 30 * 8]
    ldp x1, x2, [sp, 32 * 8]
    msr spsr_el1, x0
    msr elr_el1, x1
    msr sp_el0, x2
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
    ldp x28, fp, [sp, 28 * 8]
    add sp, sp, 34 * 8
.endm

// NOTE: the layout must be exactly the same as SwitchFrame
.global switch_context
switch_context:
    sub sp, sp, 14 * 8
    stp x19, x20, [sp, 0 * 8]
    stp x21, x22, [sp, 2 * 8]
    stp x23, x24, [sp, 4 * 8]
    stp x25, x26, [sp, 6 * 8]
    stp x27, x28, [sp, 8 * 8]
    stp fp, lr, [sp, 10 * 8]
    mrs x9, tpidr_el1
    str x9, [sp, 12 * 8]
    mov x10, sp
    str x10, [x1]

    ldr x10, [x0]
    mov sp, x10
    ldr x9, [sp, 12 * 8]
    msr tpidr_el1, x9
    ldp x19, x20, [sp, 0 * 8]
    ldp x21, x22, [sp, 2 * 8]
    ldp x23, x24, [sp, 4 * 8]
    ldp x25, x26, [sp, 6 * 8]
    ldp x27, x28, [sp, 8 * 8]
    ldp fp, lr, [sp, 10 * 8]
    add sp, sp, 14 * 8
    ret

.global kernel_space_thread_startup
kernel_space_thread_startup:
    mov x0, sp
    bl handle_signal
    load_trap_frame
    eret

.align 11
.global interrupt_vector_table
interrupt_vector_table:
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7

    b interrupt_entry_el1_cur_sync
    .align 7
    b interrupt_entry_el1_cur_irq
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7

    b interrupt_entry_el1_low_sync
    .align 7
    b interrupt_entry_el1_low_irq
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7

    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7
    b interrupt_entry_default
    .align 7

interrupt_entry_el1_cur_sync:
    save_trap_frame
    mov x0, sp
    bl handle_interrupt_el1_cur_sync
    mov x0, sp
    bl handle_signal
    load_trap_frame
    eret

interrupt_entry_el1_cur_irq:
    save_trap_frame
    mov x0, sp
    bl handle_interrupt_el1_cur_irq
    mov x0, sp
    bl handle_signal
    load_trap_frame
    eret

interrupt_entry_el1_low_sync:
    save_trap_frame
    mov x0, sp
    bl handle_interrupt_el1_low_sync
    mov x0, sp
    bl handle_signal
    load_trap_frame
    eret

interrupt_entry_el1_low_irq:
    save_trap_frame
    mov x0, sp
    bl handle_interrupt_el1_low_irq
    mov x0, sp
    bl handle_signal
    load_trap_frame
    eret

interrupt_entry_default:
    save_trap_frame
    mov x0, sp
    bl handle_interrupt_default
    load_trap_frame
    eret
