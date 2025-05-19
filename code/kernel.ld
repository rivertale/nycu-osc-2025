SECTIONS
{
    . = 0xffff000000000000;
    . += 0x80000;
    section_image_low = .;

    .text.boot : { KEEP(*(.text.boot)) }
    .text : { *(.text) }

    . = ALIGN(0x1000);
    section_bridge_low = .;
    .text.bridge : { *(.text.bridge) }
    . = ALIGN(0x1000);
    section_bridge_high = .;

    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) }

    . = ALIGN(0x10);
    section_bss_low = .;
    .bss (NOLOAD) : { *(.bss) }
    . = ALIGN(0x10);
    section_bss_high = .;
    section_bss_size = section_bss_high - section_bss_low;

    . = ALIGN(0x10);
    section_stack_low = .;
    . += 0x10000; /* 64kb stack */
    section_stack_high = .;

    section_image_high = .;
    _end = .;
}
