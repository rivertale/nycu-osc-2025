SECTIONS
{
    . = 0xffff000000000000;
    . += 0x80000;
    kernel_image_begin = .;

    .text.boot : { KEEP(*(.text.boot)) }
    .text : { *(.text) }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) }

    . = ALIGN(0x10);
    bss_begin = .;
    .bss (NOLOAD) : { *(.bss) }
    . = ALIGN(0x10);
    bss_end = .;
    bss_size = bss_end - bss_begin;

    . = ALIGN(0x10);
    stack_begin = .;
    . += 0x10000; /* 64kb stack */
    stack_end = .;

    kernel_image_end = .;
    kernel_image_size = kernel_image_end - kernel_image_begin;
    _end = .;
}
