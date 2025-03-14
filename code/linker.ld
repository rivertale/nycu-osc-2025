SECTIONS
{
    . = 0x80000;
    .text.boot : { KEEP(*(.text.boot)) }
    .text : { *(.text) }
    
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) }

    . = ALIGN(0x10);
    bss_begin = .;
    .bss (NOLOAD): { *(.bss*) }
    bss_end = .;

    . = ALIGN(0x10);
    stack_begin = .;
    . += 0x100000; /* 1mb stack */
    stack_end = .;
    _end = .;
}
