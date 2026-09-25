/**
 *
 *  File Name: user_x86_64.ld
 *
 *  Sirius Education - x86_64 User Space Application Linker Script
 *
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

/* O ponto de entrada padrão da aplicação Ring 3 (Definido no crt0.asm) */
ENTRY(_start)

/*
 * ============================================================
 * Program Headers (Mapeados nativamente para o process_create)
 * ============================================================
 */
PHDRS
{
    text   PT_LOAD FLAGS(5);   /* Read + Execute */
    rodata PT_LOAD FLAGS(4);   /* Read Only */
    data   PT_LOAD FLAGS(6);   /* Read + Write */
}

SECTIONS
{
    /*
     * ========================================================
     * User Space Canonical Base Address (Ring 3)
     * ========================================================
     */
    . = 0x00400000;

    /*
     * Primeira página reservada.
     * Alinha a entrada do código principal na fronteira limpa 
     * da página seguinte (0x401000).
     */
    . += 0x1000;

    /* Define o símbolo do Entry Point virtual da aplicação utilizadora */
    _start = .;

    /*
     * ========================================================
     * TEXT (Código Executável da Aplicação)
     * ========================================================
     */
    .text : ALIGN(0x1000)
    {
        _code = .;
        
        /* 
         * Força o código de inicialização do crt0.asm (.text.boot) 
         * a ficar no topo absoluto do binário utilizador.
         */
        KEEP(*(.text.boot))
        
        *(.text)
        *(.text.*)
        
        . = ALIGN(0x1000);
    } :text

    /*
     * ========================================================
     * READ ONLY DATA (Constantes, tabelas e strings literais)
     * ========================================================
     */
    .rodata : ALIGN(0x1000)
    {
        _rodata = .;
        
        *(.rodata)
        *(.rodata.*)
        
        /* Informações de deswind/frames da libc do utilizador */
        *(.eh_frame)
        *(.eh_frame.*)
        
        . = ALIGN(0x1000);
    } :rodata

    /*
     * ========================================================
     * DATA (Variáveis Globais Inicializadas)
     * ========================================================
     */
    .data : ALIGN(0x1000)
    {
        _data = .;
        
        *(.data)
        *(.data.*)
        
        . = ALIGN(0x1000);
    } :data

    /*
     * ========================================================
     * BSS (Variáveis Globais Não Inicializadas)
     * ========================================================
     * Nota: O seu process_create lerá o p_memsz e preencherá 
     * esta cauda com zeros na memória de forma automática.
     */
    .bss : ALIGN(0x1000)
    {
        _bss = .;
        
        *(.bss)
        *(.bss.*)
        *(COMMON)
        
        . = ALIGN(0x1000);
    } :data

    /* Símbolos lógicos de encerramento da imagem virtual do processo */
    _end = .;
    __end = .;
}