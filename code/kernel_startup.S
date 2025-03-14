.section ".text.boot"
.globl _start

_start:
    // devicetree addr
    mov x4, x0
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
    // devicetree addr
    mov x0, x4
    bl kernel_main

hang:
    wfe
    b hang
