/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: syscall.c
 *    Description: Implementação da tabela de vetores (System Call Table) e
 *                 do despachante centralizado. Programa os MSRs locais de
 *                 cada núcleo (x86_64) para habilitar a transição SCI.
 *
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 06/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/syscall/syscall.h>
#include <kernel/klib.h>

/*
 * REGS DE HARDWARE ESPECÍFICOS DA ARQUITETURA (x86_64 MSRs)
 * ------------------------------------------------------------------------
 */
#define MSR_IA32_STAR 0xC0000081UL
#define MSR_IA32_LSTAR 0xC0000082UL
#define MSR_IA32_FMASK 0xC0000084UL

/* Símbolo do ponto de entrada de baixo nível contido no syscall_stub.asm */
extern void syscall_entry_stub(void);

/*
 * ============================================================================
 * SYSTEM CALL TABLE (Vetor de Despacho de Chamadas de Sistema)
 * ============================================================================
 * Mapeia estritamente os índices lógicos (RAX) aos handlers nativos em Ring 0.
 */
static const void *sys_call_table[MAX_SYSCALLS] = {
    [SYS_READ] = sys_read,
    [SYS_WRITE] = sys_write,
    [SYS_BRK] = sys_brk,
    [SYS_EXIT] = sys_exit};

/**
 * Escrita física nos Model Specific Registers (MSR) do processador x86_64.
 */
static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t low = (uint32_t)(val & 0xFFFFFFFFFFFULL);
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__(
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
        : "memory");
}

/**
 * Manipulador mestre em C (SCI Dispatcher).
 */
uint64_t syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    /* Boundary Check: Proteção contra injeção de índices inválidos por Ring 3 */
    if (syscall_num >= MAX_SYSCALLS)
    {
        kprintf("[SCI Error] Chamada de sistema desconhecida: ID %d\n", syscall_num);
        return (uint64_t)-1; /* Retorna código de erro padrão */
    }

    /* Extrai o endereço do handler correspondente */
    uint64_t (*handler)(uint64_t, uint64_t, uint64_t) = (void *)sys_call_table[syscall_num];

    if (!handler)
    {
        kprintf("[SCI Error] Handler nulo para a syscall: ID %d\n", syscall_num);
        return (uint64_t)-1;
    }

    /* Transfere a execução para o serviço interno do Kernel em Ring 0 */
    return handler(arg1, arg2, arg3);
}

/*
 * ============================================================================
 * SERVIÇOS NATIVOS INTERNOS DO KERNEL (HANDLERS)
 * ============================================================================
 */

uint64_t sys_read(void)
{
    kprintf("[SCI] sys_read executada com sucesso.\n");
    return 0;
}

uint64_t sys_write(const char *buffer, uint64_t length)
{
    /*
     * NOTA DE SEGURANÇA FUTURA:
     * Validar se os limites virtuais de 'buffer' residem estritamente no
     * Espaço de Utilizador (< KERNEL_VIRTUAL_BASE) para evitar exploit de leitura.
     */
    for (uint64_t i = 0; i < length; i++)
    {
        kprintf("%c", buffer[i]);
    }
    return length;
}

uint64_t sys_brk(void *addr)
{
    kprintf("[SCI] sys_brk: Solicitacao para expandir Heap ate 0x%lx\n", (uint64_t)addr);
    /* A integração com a PCB (heap_end) e paginação dinâmica entrará aqui */
    return 0;
}

/*
 * ============================================================================
 * INTERFACE DE INICIALIZAÇÃO DE HARDWARE
 * ============================================================================
 */

void syscall_init(void)
{
    /*
     * Habilitação de Extensões do Processador via MSR (EFER):
     * Evita a exceção 'Invalid Opcode' (#UD) ao ativar recursos avançados da CPU.
     *
     * Registador IA32_EFER (Extended Feature Enable Register) -> Endereço: 0xC0000080
     * - Bit 0  (SCE): System Call Extensions (Habilita as instruções SYSCALL/SYSRET).
     * - Bit 11 (NXE): No-Execute Enable (Habilita a proteção de páginas XD/NX).
     */
    uint32_t eax, edx;
    __asm__ __volatile__(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        : "=a"(eax), "=d"(edx) : : "ecx");

    eax |= (1U << 11); // Ativa o bit 11 (NXE)
    eax |= (1U << 0);  // Ativa o bit 0 (SCE)

    __asm__ __volatile__(
        "wrmsr\n"
        : : "a"(eax), "d"(edx), "c"(0xC0000080) : "memory");

    /* ========================================================================
     * IA32_STAR
     *
     * Kernel:
     *
     *   CS = 0x08
     *
     * User SYSRET:
     *
     *   SS = 0x2B
     *   CS = 0x33
     *
     * GDT:
     *
     *   0x28 = Sysret Data
     *   0x30 = Sysret Code
     * ======================================================================== */

    uint64_t star =
        ((uint64_t)0x08 << 32) |
        ((uint64_t)0x23 << 48);

    wrmsr(MSR_IA32_STAR, star);

    /*
     * 2. CONFIGURAÇÃO DO MSR LSTAR (Handler de Baixo Nível)
     */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry_stub);

    /*
     * 3. CONFIGURAÇÃO DO MSR FMASK (Máscara de Proteção das Flags)
     */
    wrmsr(MSR_IA32_FMASK, 0x200UL);
}