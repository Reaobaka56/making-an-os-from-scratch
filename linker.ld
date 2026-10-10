OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    __text_start = .;
    .text : { *(.text .text.*) }

    . = ALIGN(4K);
    __text_end = .;
    __rodata_start = .;
    .rodata : { *(.rodata .rodata.*) }

    . = ALIGN(4K);
    __rodata_end = .;
    __data_start = .;
    .data : {
        *(.data .data.*)
        *(.limine_requests)
    }

    . = ALIGN(4K);
    .bss : { *(.bss .bss.*) *(COMMON) }
    . = ALIGN(4K);
    __data_end = .;

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) *(.comment) }
}
