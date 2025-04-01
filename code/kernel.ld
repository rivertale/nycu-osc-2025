SECTIONS
{
    . = 0x80000;
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
    heap_begin = .;
    heap_end = 0x200000;
    heap_size = heap_end - heap_begin;

    . = ALIGN(0x10);
    stack_begin = .;
    stack_end = 0x300000;

    _end = .;
}
