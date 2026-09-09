/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: shm.c
 *    Description: Implementação de Memória Partilhada (Shared Memory) para IPC.
 *                 Gere a alocação de frames físicos globais, mapeamento dinâmico
 *                 nas tabelas de páginas (PML4) e a libertação de recursos.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/mm/memory_map.h>
#include <kernel/kernel/ipc/ipc.h>
#include <kernel/klib.h>

#define MAX_SHM_SEGMENTS 16
#define SHM_CREAT        0x0200
#define USER_SHM_BASE    USER_HEAP_VIRTUAL_BASE

/* Estrutura interna que define um segmento físico partilhado pelo Kernel */
typedef struct {
    int id;                     /* Identificador único do segmento IPC */
    unsigned long phys_frame;   /* Endereço do frame físico alocado via PMM */
    int ref_count;              /* Contador de referências (processos atracados) */
    int is_active;              /* Flag de estado de ocupação do slot na tabela */
} shm_segment_t;

/* Tabela estática global que mantém o rasto das SHMs ativas no sistema */
static shm_segment_t g_shm_table[MAX_SHM_SEGMENTS];
static int g_next_shm_id = 1;

/**
 * Obtém ou instancia um identificador de memória partilhada global.
 * 
 * @param key   Chave numérica identificadora escolhida pelo utilizador.
 * @param flags Opções de criação (ex: SHM_CREAT).
 * @return ID do segmento SHM criado/encontrado ou -1 em caso de erro.
 */
int sys_shm_get(int key, int flags) 
{
    /* Varre a tabela global à procura de uma chave já existente */
    for (int i = 0; i < MAX_SHM_SEGMENTS; i++) 
    {
        if (g_shm_table[i].is_active && g_shm_table[i].id == key)
        {
            return g_shm_table[i].id;
        }
    }

    /* Caso não exista e a flag de criação esteja activa, aloca um novo segmento */
    if (flags & SHM_CREAT) 
    {
        for (int i = 0; i < MAX_SHM_SEGMENTS; i++) 
        {
            if (!g_shm_table[i].is_active) 
            {
                /* Solicita uma página física livre ao Physical Memory Manager */
                unsigned long phys = pmm_alloc_page();
                if (!phys) return -1;

                /* Limpa buffers residuais na página física mapeando-a temporariamente */
                memset(vmm_scratch_map(phys), 0, 4096);

                /* Inicializa os metadados do segmento global */
                g_shm_table[i].id = g_next_shm_id++;
                g_shm_table[i].phys_frame = phys;
                g_shm_table[i].ref_count = 0;
                g_shm_table[i].is_active = 1;
                
                return g_shm_table[i].id;
            }
        }
    }
    return -1;
}

/**
 * Acopla (mapeia) o segmento físico de SHM no espaço virtual do processo atual.
 * 
 * @param shmid Identificador do segmento obtido via sys_shm_get.
 * @param proc  Ponteiro para o PCB do processo que está a invocar a operação.
 * @return Ponteiro virtual gerado no espaço de utilizador ou (void*)-1 se falhar.
 */
void* sys_shm_at(int shmid, process_t* proc) 
{
    if (!proc) return (void*)-1;

    /* Procura o segmento correspondente na tabela global do Kernel */
    shm_segment_t* seg = NULL;
    for (int i = 0; i < MAX_SHM_SEGMENTS; i++) 
    {
        if (g_shm_table[i].is_active && g_shm_table[i].id == shmid) 
        {
            seg = &g_shm_table[i];
            break;
        }
    }
    if (!seg) return (void*)-1;

    /* Encontra um slot livre no vetor local de SHM do próprio processo */
    int slot = -1;
    for (int i = 0; i < MAX_SHARED_REGIONS; i++) 
    {
        if (proc->shm_ids[i] == 0) 
        {
            slot = i;
            break;
        }
    }
    if (slot == -1) return (void*)-1;

    /* Calcula o endereço virtual linear onde a página será injetada */
    unsigned long virt_addr = USER_SHM_BASE + (slot * 4096);
    
    /* Configura bits de permissão x86_64: User-Supervisor (0x4) | Read-Write (0x2) */
    unsigned long flags = 0x6; 

    /* Injeta o mapeamento físico na árvore PML4 privada do processo usando o scratch virtual */
    vmm_map_page((PML4_TABLE*)vmm_scratch_map(proc->cr3), virt_addr, seg->phys_frame, flags);

    /* Atualiza o histórico local do processo e incrementa o uso do frame global */
    proc->shm_ids[slot] = shmid;
    proc->shm_virtual_addresses[slot] = (void*)virt_addr;
    seg->ref_count++;

    return (void*)virt_addr;
}

/**
 * Desvincula um segmento de memória partilhada decrementando as suas referências.
 * Se o contador de referências chegar a zero, a página física é devolvida ao PMM.
 * 
 * @param shmid Identificador do segmento a ser desativado ou limpo.
 */
void detach_shm_segment(int shmid)
{
    for (int i = 0; i < MAX_SHM_SEGMENTS; i++)
    {
        if (g_shm_table[i].is_active && g_shm_table[i].id == shmid)
        {
            /* Decrementa o uso do segmento */
            g_shm_table[i].ref_count--;

            /* Se mais nenhum processo está acoplado, liberta a página física */
            if (g_shm_table[i].ref_count <= 0)
            {
                pmm_free_page(g_shm_table[i].phys_frame);
                
                /* Limpa completamente o slot global para reutilização futura */
                g_shm_table[i].phys_frame = 0;
                g_shm_table[i].id = 0;
                g_shm_table[i].ref_count = 0;
                g_shm_table[i].is_active = 0;
                
                kprintf("[IPC SHM] Segmento %d totalmente desalocado do Kernel.\n", shmid);
            }
            return;
        }
    }
}