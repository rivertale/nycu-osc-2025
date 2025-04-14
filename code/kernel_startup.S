#include "kernel_interrupt.S"

.section ".text.boot"
.globl _start
_start:
    // store devicetree addr
    mov x4, x0

    bl from_el2_to_el1

    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbnz x0, hang
    ldr x0, =stack_end
    mov sp, x0

    // clear bss
    ldr x0, =bss_begin
    ldr x1, =bss_size
    mov x2, #0
clear_bss_loop:
    cbz x1, clear_bss_done
    str x2, [x0], #8
    sub x1, x1, #8
    b clear_bss_loop
clear_bss_done:
    adr x0, interrupt_vector_table
    msr vbar_el1, x0

    // restore devicetree addr
    mov x0, x4
    bl kernel_main

hang:
    wfe
    b hang

from_el2_to_el1:
    mov x0, (1 << 31)
    msr hcr_el2, x0
    mov x0, #0x345
    msr spsr_el2, x0
    msr elr_el2, lr
    eret
