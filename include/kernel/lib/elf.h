/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: elf.h
 *    Description: Definições do formato binário ELF64 e tabelas de símbolos..
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _ELF_H_
#define _ELF_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

#define ELF_MAGIC_0 0x7F
#define ELF_MAGIC_1 'E'
#define ELF_MAGIC_2 'L'
#define ELF_MAGIC_3 'F'

#define PT_LOAD     1  /* Tipo de segmento: Mapeável na memória virtual RAM */

/* 
 * Constantes do Formato ELF64 
 */
#define EI_NIDENT       16

/* Tipos de Secções (sh_type) */
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_NOBITS      8
#define SHT_REL         9

/* Flags de Secções (sh_flags) */
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4

/* Tipos de Relocação específicos para x86_64 */
#define R_X86_64_NONE   0
#define R_X86_64_64     1
#define R_X86_64_PC32   2
#define R_X86_64_PLT32  4
#define R_X86_64_32     10
#define R_X86_64_32S    11

/* Macros para decodificação de Relocações e Símbolos ELF64 */
#define ELF64_R_SYM(i)    ((i) >> 32)
#define ELF64_R_TYPE(i)   ((i) & 0xffffffffL)
#define ELF64_ST_BIND(i)  ((i) >> 4)
#define ELF64_ST_TYPE(i)  ((i) & 0xf)

/*
 * Estruturas Oficiais do Cabeçalho ELF64
 */
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf64_Ehdr;

/* Cabeçalho de Segmento de Programa (PHDR - Tamanho: 56 bytes) */
typedef struct {
    uint32_t p_type;       /* Tipo de segmento (1 = PT_LOAD) */
    uint32_t p_flags;      /* Permissões: 1=Execução, 2=Escrita, 4=Leitura */
    uint64_t p_offset;     /* Onde o segmento começa fisicamente dentro do disco */
    uint64_t p_vaddr;      /* Endereço Virtual Alvo na RAM (Onde o programa vai rodar) */
    uint64_t p_paddr;      /* Endereço Físico Alvo (Ignorado com paginação ativa) */
    uint64_t p_filesz;     /* Tamanho do segmento em bytes dentro do ficheiro no disco */
    uint64_t p_memsz;      /* Tamanho do segmento na memória virtual (Se memsz > filesz, .bss) */
    uint64_t p_align;      /* Alinhamento de página (normalmente 0x1000 = 4KB) */
} __attribute__((packed)) Elf64_Phdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t      st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t      st_shndx;
    uint64_t      st_value;
    uint64_t      st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
} Elf64_Rel;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;


#endif /* _ELF_H_ */