.globl _start

.section ".text.reloc"
_start:
    // devicetree addr
    mov x4, x0

    // reloc
    ldr x0, =reloc_begin
    ldr x1, =reloc_from
    ldr x2, =reloc_size
reloc_loop:
    cbz x2, reloc_done
    ldr x3, [x1], #8
    str x3, [x0], #8
    sub x2, x2, #8
    b reloc_loop
reloc_done:
    ldr x0, =reloc_begin
    br x0

.section ".text.boot"
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
    bl bootloader_main

hang:
    wfe
    b hang
