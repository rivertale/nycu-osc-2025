.section ".text.boot"
.globl _start
_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbnz x0, hang
    mov x0, #0
    ldr x1, =bss_begin
    ldr x2, =bss_end
clear_bss:
    cmp x1, x2
    bge clear_bss_done
    str x0, [x1], #8
    b clear_bss
clear_bss_done:
    ldr x0, =stack_end
    mov sp, x0
    bl kernel_main
hang:
    wfe
    b hang
