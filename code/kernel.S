#include "kernel_interrupt.S"

.section ".text.boot"
.global _start
_start:
    // store devicetree addr
    mov x4, x0

    bl from_el2_to_el1

    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbnz x0, hang
    ldr x0, =stack_end
    mov sp, x0

    ldr x0, =(0 << 14) | (2 << 30) | ((64 - 48) << 0) | ((64 - 48) << 16)
    msr tcr_el1, x0
    
    ldr x0, =(0 << 0) | (0x44 << 8)
    msr mair_el1, x0

    mov x0, 0x1000
    mov x1, 0x2000
    ldr x2, =3
    orr x2, x1, x2
    str x2, [x0]
    
    ldr x2, =(1 << 10) | (0 << 2) | 1
    mov x3, 0x00000000
    orr x3, x2, x3
    str x3, [x1]
    
    mov x3, 0x40000000
    orr x3, x2, x3
    str x3, [x1, 8]
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0
    
    mrs x2, sctlr_el1
    orr x2, x2, 1
    msr sctlr_el1, x2

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
    ldr x0, =interrupt_vector_table
    msr vbar_el1, x0

    // restore devicetree physical address
    mov x0, x4
    ldr x1, =kernel_main
    br x1

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
