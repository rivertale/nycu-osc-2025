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
    . += 0x100000; /* 1mb heap */
    . = ALIGN(0x10);
    heap_end = .;
    heap_size = heap_end - heap_begin;

    . = ALIGN(0x10);
    stack_begin = .;
    . += 0x100000; /* 1mb stack */
    . = ALIGN(0x10);
    stack_end = .;

    _end = .;
}
