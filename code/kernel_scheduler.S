.global get_current_thread
get_current_thread:
    mrs x0, tpidr_el1
    ret

.global set_current_thread
set_current_thread:
    msr tpidr_el1, x0
    ret

.global switch_thread
switch_thread:
    
    mov x9, sp
    mrs x10, tpidr_el1
    stp x9, x10, [x1, 16 * 0]
    stp x19, x20, [x1, 16 * 1]
    stp x21, x22, [x1, 16 * 2]
    stp x23, x24, [x1, 16 * 3]
    stp x25, x26, [x1, 16 * 4]
    stp x27, x28, [x1, 16 * 5]
    stp fp, lr, [x1, 16 * 6]

    ldp x9, x10, [x0, 16 * 0]
    ldp x19, x20, [x0, 16 * 1]
    ldp x21, x22, [x0, 16 * 2]
    ldp x23, x24, [x0, 16 * 3]
    ldp x25, x26, [x0, 16 * 4]
    ldp x27, x28, [x0, 16 * 5]
    ldp fp, lr, [x0, 16 * 6]
    mov sp, x9
    msr tpidr_el1, x10
    ret
