/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: elf.h
 *    Description: Cabeçalho do Carregador Nativo de Executáveis ELF64.
 *                 Define estruturas de cabeçalho (EHDR) e segmentos (PHDR).
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 13/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _ELF_H_
#define _ELF_H_

#include <kernel/lib/stdint.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/kernel/sched/scheduler.h>

#define ELF_MAGIC_0 0x7F
#define ELF_MAGIC_1 'E'
#define ELF_MAGIC_2 'L'
#define ELF_MAGIC_3 'F'

#define PT_LOAD     1  /* Tipo de segmento: Mapeável na memória virtual RAM */

/* Cabeçalho Principal do Ficheiro ELF64 (EHDR - Tamanho: 64 bytes) */
typedef struct {
    uint8_t  e_ident[16];  /* Assinatura Mágica de Validação (0x7F 'E' 'L' 'F') */
    uint16_t e_type;       /* Tipo do objeto (2 = Executável) */
    uint16_t e_machine;    /* Arquitetura Alvo (0x3E = x86_64) */
    uint32_t e_version;    /* Versão do formato (Sempre 1) */
    uint64_t e_entry;      /* Endereço Virtual do Ponto de Entrada (Entry Point RIP) */
    uint64_t e_phoff;      /* Deslocamento da Tabela de Segmentos de Programa */
    uint64_t e_shoff;      /* Deslocamento da Tabela de Seções */
    uint32_t e_flags;        
    uint16_t e_ehsize;     /* Tamanho deste cabeçalho (64 bytes) */
    uint16_t e_phentsize;  /* Tamanho de cada entrada na tabela de Programa */
    uint16_t e_phnum;      /* Quantidade de Segmentos a carregar na RAM */
    uint16_t e_shentsize;    
    uint16_t e_shnum;        
    uint16_t e_shstrndx;     
} __attribute__((packed)) elf64_ehdr_t;

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
} __attribute__((packed)) elf64_phdr_t;

/**
 * Lê um binário executável do disco para um buffer alinhado via pool_alloc 
 * e envia-o para ser instanciado como um novo processo no Escalonador.
 */
process_t* elf_load_and_create_process(const char* path, int argc, char** argv, uint32_t cpu_id);

#endif /* _ELF_H_ */