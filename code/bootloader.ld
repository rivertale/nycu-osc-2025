SECTIONS
{
    . = 0x80000;
    .text.reloc : { KEEP(*(.text.reloc)) }

    reloc_from = .;
    . = 0x40000;
    . = ALIGN(0x10);
    reloc_begin = .;
    .text.boot : AT(. - reloc_begin + reloc_from)
    {
        KEEP(*(.text.boot))
    }
    .text : AT(. - reloc_begin + reloc_from)
    {
        *(.text)
    }
    .rodata : AT(. - reloc_begin + reloc_from)
    {
        *(.rodata)
        *(.rodata.*)
    }
    .data : AT(. - reloc_begin + reloc_from)
    {
        *(.data)
    }
    . = ALIGN(0x10);
    reloc_end = .;
    reloc_size = reloc_end - reloc_begin;

    bss_begin = .;
    .bss (NOLOAD): { *(.bss) }
    . = ALIGN(0x10);
    bss_end = .;
    bss_size = bss_end - bss_begin;

    . = ALIGN(0x10);
    stack_begin = .;
    . += 0x100000; /* 1mb stack */
    . = ALIGN(0x10);
    stack_end = .;

    _end = .;
}
