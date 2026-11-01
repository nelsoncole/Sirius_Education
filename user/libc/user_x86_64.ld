/**
 *
 *  File Name: user_x86_64.ld
 *
 *  Sirius Education - x86_64 User Space Application Linker Script
 *
 */


OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00400000;

    . += 0x1000;

    _start = .;

    /* Executable code */
    .text ALIGN(0x1000) :
    {
        _code = .;

        KEEP(*(.text.boot))
        KEEP(*(.text.boot.*))

        *(.text .text.*)

        . = ALIGN(0x1000);
        _etext = .;
    } :text

    /* Read-only data */
    .rodata ALIGN(0x1000) :
    {
        _rodata = .;

        *(.rodata .rodata.*)
        *(.srodata .srodata.*)

        *(.eh_frame_hdr)
        *(.eh_frame .eh_frame.*)

        . = ALIGN(0x1000);
        _erodata = .;
    } :rodata

    /* Writable initialized data */
    .data ALIGN(0x1000) :
    {
        _data = .;

        *(.data .data.*)
        *(.sdata .sdata.*)

        *(.got)
        *(.got.*)
        *(.got.plt)

        *(.data.rel.ro .data.rel.ro.*)

        . = ALIGN(0x1000);
        _edata = .;
    } :data

    /* Constructor arrays */
    .preinit_array ALIGN(8) :
    {
        __preinit_array_start = .;
        KEEP(*(.preinit_array))
        KEEP(*(.preinit_array.*))
        __preinit_array_end = .;
    } :data

    .init_array ALIGN(8) :
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } :data

    .fini_array ALIGN(8) :
    {
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } :data

    /* Uninitialized data */
    .bss ALIGN(0x1000) (NOLOAD) :
    {
        _bss = .;

        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)

        . = ALIGN(0x1000);

        _ebss = .;
    } :data

    /* End of process image */
    . = ALIGN(0x1000);

    _end = .;
    __end = .;
    _end_of_image = .;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
    }
}