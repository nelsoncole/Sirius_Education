/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: loader.c
 *    Description: Carregador e analisador de ficheiros binários no formato
 *                 ELF64 Relocável para execução no espaço de memória do Kernel.
 *
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kmods/kmod.h>
#include <kernel/klib.h>

/*
 * Valida se o buffer fornecido aponta para um ficheiro ELF64 x86_64 válido.
 */
static bool elf_validate_header(Elf64_Ehdr *ehdr)
{
    /* Validar a assinatura mágica do formato ELF (0x7F, 'E', 'L', 'F') */
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
    {
        kprintf("[kmod]: Assinatura ELF invalida.\n");
        return false;
    }

    /* Validar se o binário é da classe de 64-bits (Classe 2 = ELFCLASS64) */
    if (ehdr->e_ident[4] != 2)
    {
        kprintf("[kmod]: O ficheiro nao e ELF 64-bits.\n");
        return false;
    }

    /* Validar a arquitetura alvo (Machine 62 = EM_X86_64) */
    if (ehdr->e_machine != 62)
    {
        kprintf("[kmod]: Arquitetura alvo nao suportada (Requer x86_64).\n");
        return false;
    }

    /* Validar o tipo de ficheiro (Type 1 = ET_REL, Relocável) */
    if (ehdr->e_type != 1)
    {
        kprintf("[kmod]: O modulo tem de ser compilado como ficheiro relocavel (-r).\n");
        return false;
    }

    return true;
}

/*
 * Rotina principal de carregamento dinâmico de módulos (LKM).
 */
int kmod_load(const uint8_t *elf_buffer, size_t size)
{
    if (!elf_buffer || size < sizeof(Elf64_Ehdr))
    {
        kprintf("[kmod]: Erro: Buffer de modulo invalido ou corrompido.\n");
        return -1;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_buffer;

    /* Executa as validações estruturais do formato */
    if (!elf_validate_header(ehdr))
    {
        return -2;
    }

    /* Localiza a tabela de cabeçalho de secções (Section Headers) */
    Elf64_Shdr *shdr = (Elf64_Shdr *)(elf_buffer + ehdr->e_shoff);

    /* Obtém a secção de strings que contém os nomes das secções (.text, .data, etc) */
    Elf64_Shdr *shstrtab_sec = &shdr[ehdr->e_shstrndx];
    const char *shstrtab = (const char *)(elf_buffer + shstrtab_sec->sh_offset);

    kprintf("[kmod]: A analisar modulo contendo %d seccoes...\n", ehdr->e_shnum);

    /*
     * Primeiro Passo: Alocar espaço físico em memória RAM do Kernel para todas
     * as secções marcadas com SHF_ALLOC (.text, .data, .rodata, .bss) e carregar dados.
     */
    for (size_t i = 0; i < ehdr->e_shnum; i++)
    {
        const char *section_name = &shstrtab[shdr[i].sh_name];

        /* Ignora secções que não requerem carregamento em memória */
        if (!(shdr[i].sh_flags & SHF_ALLOC) || shdr[i].sh_size == 0)
        {
            continue;
        }

        /* Aloca o bloco de memória para a secção atual */
        void *mem = kmalloc(shdr[i].sh_size);
        if (!mem)
        {
            kprintf("[kmod]: Erro ao alocar memoria para a seccao %s.\n", section_name);
            return -3;
        }

        /* Se a secção contiver dados binários, copia-os; se for BSS, limpa a zero */
        if (shdr[i].sh_type == SHT_PROGBITS)
        {
            memcpy(mem, elf_buffer + shdr[i].sh_offset, shdr[i].sh_size);
            if (shdr[i].sh_flags & SHF_EXECINSTR)
            {
                kprintf(" -> Carregada seccao de codigo: %s (%d bytes)\n", section_name, shdr[i].sh_size);
            }
            else
            {
                kprintf(" -> Carregada seccao de dados: %s (%d bytes)\n", section_name, shdr[i].sh_size);
            }
        }
        else if (shdr[i].sh_type == SHT_NOBITS)
        {
            memset(mem, 0, shdr[i].sh_size);
            kprintf(" -> Inicializada seccao BSS: %s (%d bytes)\n", section_name, shdr[i].sh_size);
        }

        /* Atualiza o endereço virtual oficial da secção com o ponteiro do kmalloc */
        shdr[i].sh_addr = (uint64_t)mem;
    }

    /*
     * Segundo Passo: Localizar as tabelas de símbolos (.symtab) e a tabela de strings (.strtab).
     */
    Elf64_Sym *symtab = NULL;
    const char *strtab = NULL;
    size_t sym_count = 0;

    for (size_t i = 0; i < ehdr->e_shnum; i++)
    {
        if (shdr[i].sh_type == SHT_SYMTAB)
        {
            symtab = (Elf64_Sym *)(elf_buffer + shdr[i].sh_offset);
            sym_count = shdr[i].sh_size / sizeof(Elf64_Sym);

            Elf64_Shdr *strtab_sec = &shdr[shdr[i].sh_link];
            strtab = (const char *)(elf_buffer + strtab_sec->sh_offset);
            break;
        }
    }

    /*
     * Terceiro Passo: Processar e aplicar as tabelas de Relocação (.rela) a 100% em 64-bits.
     */
    for (size_t i = 0; i < ehdr->e_shnum; i++)
    {
        if (shdr[i].sh_type != SHT_RELA)
        {
            continue;
        }

        Elf64_Shdr *target_sec = &shdr[shdr[i].sh_info];
        const char *target_name = &shstrtab[target_sec->sh_name];

        /* Ignora tabelas de depuração/exceção do GCC nativo (.eh_frame) */
        if (strcmp(target_name, ".eh_frame") == 0)
        {
            continue;
        }

        uintptr_t target_base = (uintptr_t)target_sec->sh_addr;

        /* Se a secção alvo não foi alocada na memória, ignora as relocações dela */
        if (!target_base)
        {
            continue;
        }

        Elf64_Rela *rela = (Elf64_Rela *)(elf_buffer + shdr[i].sh_offset);
        size_t rela_count = shdr[i].sh_size / sizeof(Elf64_Rela);

        for (size_t j = 0; j < rela_count; j++)
        {
            uint32_t sym_idx = ELF64_R_SYM(rela[j].r_info);
            uint32_t rel_type = ELF64_R_TYPE(rela[j].r_info);

            Elf64_Sym *sym = &symtab[sym_idx];
            uintptr_t sym_addr = 0;

            /* Resolve símbolos internos ou externos do Kernel com proteção de limites */
            if (sym->st_shndx != 0 && sym->st_shndx < ehdr->e_shnum)
            {
                Elf64_Shdr *sym_sec = &shdr[sym->st_shndx];
                sym_addr = (uintptr_t)(sym_sec->sh_addr + sym->st_value);
            }
            else
            {
                const char *sym_name = &strtab[sym->st_name];
                sym_addr = kmod_find_symbol(sym_name);

                if (!sym_addr)
                {
                    kprintf("[kmod]: Erro: Simbolo externo '%s' nao encontrado no Kernel.\n", sym_name);
                    return -4;
                }
            }

            uintptr_t patch_ptr = target_base + rela[j].r_offset;

            switch (rel_type)
            {
            case R_X86_64_64:
                /* 
                 * REMENDO PURE 64-BITS: Escreve o endereço completo de 64 bits na RAM.
                 * Graças ao -mcmodel=large do teu Makefile, chamadas externas como kprintf 
                 * usam este caso e suportam distâncias ilimitadas até à memória alta.
                 */
                *(uint64_t *)patch_ptr = sym_addr + rela[j].r_addend;
                break;

            case R_X86_64_32:
            case 11: /* R_X86_64_32S - Patch de 32 bits com sinal para offsets curtos locais */
                *(uint32_t *)patch_ptr = (uint32_t)(sym_addr + rela[j].r_addend);
                break;

            case R_X86_64_PC32:
            case R_X86_64_PLT32:
            {
                /* Usado estritamente para saltos relativos internos dentro do próprio módulo */
                intptr_t delta = (intptr_t)(sym_addr + rela[j].r_addend - patch_ptr);
                *(uint32_t *)patch_ptr = (uint32_t)delta;
                break;
            }

            default:
                kprintf("[kmod]: Erro: Tipo de relocacao %d nao suportado no simbolo '%s'.\n", 
                        (int)rel_type, &strtab[sym->st_name]);
                return -5;
            }
        }
    }

    /*
     * Quarto Passo: Encontrar e mapear as funções 'module_init' e 'module_exit'.
     * Proteção contra estouro de limites em índices especiais (SHN_ABS).
     */
    mod_init_t init_func = NULL;
    mod_exit_t exit_func = NULL;

    for (size_t i = 0; i < sym_count; i++)
    {
        const char *sym_name = &strtab[symtab[i].st_name];
        uint16_t sec_idx = symtab[i].st_shndx;

        if (sec_idx == 0 || sec_idx >= ehdr->e_shnum)
        {
            continue;
        }

        if (strcmp(sym_name, "module_init") == 0)
        {
            Elf64_Shdr *sym_sec = &shdr[sec_idx];
            init_func = (mod_init_t)(sym_sec->sh_addr + symtab[i].st_value);
        }
        else if (strcmp(sym_name, "module_exit") == 0)
        {
            Elf64_Shdr *sym_sec = &shdr[sec_idx];
            exit_func = (mod_exit_t)(sym_sec->sh_addr + symtab[i].st_value);
        }
    }

    if (!init_func)
    {
        kprintf("[kmod]: Erro: Funcao 'module_init' nao encontrada no modulo.\n");
        return -6;
    }

    /* Executa o módulo dinâmico em Ring 0 */
    kprintf("[kmod]: A saltar para module_init()...\n");
    int status = (*init_func)();

    if (status != 0)
    {
        kprintf("[kmod]: Modulo retornou erro %d na inicializacao.\n", status);
        return status;
    }

    /*
     * Quinto Passo: Registar e rastrear o módulo de forma segura no gestor.
     */
    module_t *new_mod = (module_t *)kmalloc(sizeof(module_t));
    if (!new_mod)
    {
        kprintf("[kmod]: Alerta: Modulo ativo, mas falhou o registo de gestao na RAM.\n");
        return 0;
    }

    strcpy(new_mod->name, "sample_mod");
    new_mod->init = init_func;
    new_mod->exit = exit_func;
    new_mod->state = 1;

    /* Contabilizar apenas secções válidas alocadas via kmalloc */
    size_t allocs = 0;
    for (size_t i = 0; i < ehdr->e_shnum; i++)
    {
        if (shdr[i].sh_addr != 0 && (shdr[i].sh_flags & SHF_ALLOC))
        {
            allocs++;
        }
    }
    new_mod->alloc_count = allocs;
    new_mod->section_allocs = (void **)kmalloc(sizeof(void *) * allocs);
    if (new_mod->section_allocs)
    {
        size_t idx = 0;
        for (size_t i = 0; i < ehdr->e_shnum; i++)
        {
            if (shdr[i].sh_addr != 0 && (shdr[i].sh_flags & SHF_ALLOC))
            {
                new_mod->section_allocs[idx++] = (void *)shdr[i].sh_addr;
            }
        }
    }
    
    kmod_register_tracked_module(new_mod);
    return 0;
}