/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tmpnam.c
 *    Description: Gerador de nomes de ficheiros temporários únicos em Ring 3.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 *   Modified Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Contador incremental local para garantir caminhos únicos por chamada
static volatile uint32_t g_tmpnam_counter = 0;

// Declaração externa do limite do heap para gerar entropia no nome
extern uint64_t g_uheap_current_end;

/**
 * sysgettmpnam - Gera o nome de forma nativa na memória da LibC (Ring 3).
 * @s: Buffer de destino que receberá a string do caminho temporário.
 */
static void sysgettmpnam(char *s)
{
    // 1. Obtém o estado atual do contador e incrementa-o de forma estável
    uint32_t current_id = __atomic_load_n(&g_tmpnam_counter, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&g_tmpnam_counter, 1, __ATOMIC_SEQ_CST);

    // 2. Mistura com o endereço do Heap atual para garantir aleatoriedade (entropia)
    current_id ^= (uint32_t)g_uheap_current_end;

    // 3. Formata o nome do ficheiro oculto dentro do teu diretório padrão de sistema
    // Produz caminhos como: "/sys/.tmpnam_1a2b"
    sprintf(s, "/sys/.tmpnam_%lx", (unsigned long)current_id);
}

/**
 * tmpnam - Cria um nome único para um ficheiro temporário.
 * @s: Ponteiro para o buffer de destino, ou NULL para usar o buffer interno.
 * @return: Ponteiro para a string com o nome gerado, ou NULL em caso de erro.
 */
char *tmpnam(char *s)
{
    // Buffer interno alocado no segmento de dados para chamadas com NULL.
    static char static_tmpbuffer[L_tmpnam];

    if (s == NULL) 
    {
        memset(static_tmpbuffer, 0, L_tmpnam);
        sysgettmpnam(static_tmpbuffer);
        return static_tmpbuffer;
    }
    
    sysgettmpnam(s);
    return s;
}