/**
 *
 *  File Name: x86_64.ld
 *
 *  Sirius Education - x86_64 Kernel
 *
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)


/*
 * ============================================================
 * Program Headers
 * ============================================================
 *
 * text -> Read + Execute
 * rodata -> Read only
 * data -> Read + Write
 *
 */

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R + X */
    rodata PT_LOAD FLAGS(4);   /* R */
    data   PT_LOAD FLAGS(6);   /* R + W */
}


SECTIONS
{
    /*
     * ========================================================
     * Kernel base address
     * ========================================================
     */

    . = 0xFFFFFFFF80400000;

    /*
     * Primeira página reservada.
     *
     * O Entry Point ficará em:
     *
     *     0x400000 + 0x1000 = 0x401000
     */

    . += 0x1000;


    /*
     * ========================================================
     * Kernel Entry Point
     * ========================================================
     */

    _start = .;


    /*
     * ========================================================
     * TEXT
     * ========================================================
     */

    .text : ALIGN(0x1000)
    {
        code = .;
        _code = .;
        __code = .;

        /*
         * Código de entrada do kernel
         */
        KEEP(*(.text.boot))

        /*
         * Restante do código
         */
        *(.text)
        *(.text.*)

        . = ALIGN(0x1000);
    } :text


    /*
     * ========================================================
     * READ ONLY DATA
     * ========================================================
     */

    .rodata : ALIGN(0x1000)
    {
        rodata = .;
        _rodata = .;
        __rodata = .;

        *(.rodata)
        *(.rodata.*)

        /*
         * Exception handling / unwind information
         */
        *(.eh_frame)
        *(.eh_frame.*)

        . = ALIGN(0x1000);
    } :rodata


    /*
     * ========================================================
     * DATA
     * ========================================================
     */

    .data : ALIGN(0x1000)
    {
        data = .;
        _data = .;
        __data = .;

        *(.data)
        *(.data.*)

        . = ALIGN(0x1000);
    } :data


    /*
     * ========================================================
     * BSS
     * ========================================================
     */

    .bss : ALIGN(0x1000)
    {
        bss = .;
        _bss = .;
        __bss = .;

        *(.bss)
        *(.bss.*)

        *(COMMON)

        . = ALIGN(0x1000);
    } :data


    /*
     * ========================================================
     * Kernel End
     * ========================================================
     */

    kernel_end = .;

    end = .;
    _end = .;
    __end = .;
}