/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: icmp.c
 *    Description: Subsistema ICMP (Internet Control Message Protocol).
 *                 Gere mensagens de controlo e erro da Camada 3, processa 
 *                 pedidos de Echo Request (Ping) e emite Echo Replies.
 * 
 *         Author: Nelson Cole
 *   Created Date: 20/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 20/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/kernel/net/net.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>
#include <kernel/arch/x86_64/kapi/timer.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;         /* Tipo de mensagem ICMP (ex: 8 = Request, 0 = Reply) */
    uint8_t  code;         /* Código interno da mensagem (para Echo é sempre 0) */
    uint16_t checksum;     /* Internet Checksum acumulado do payload ICMP */
    uint16_t id;           /* Identificador de sessão do Ping */
    uint16_t sequence;     /* Número de sequência incremental do pacote */
} icmp_header_t;
#pragma pack(pop)

/* Declaração externa da função do seu IP Output corrigida anteriormente */
int ip_output(uint32_t dest_ip, uint8_t protocol, const void* data, uint32_t len);
extern uint16_t ip_calculate_checksum(void* data, size_t length);
extern uint64_t g_current_packet_tsc_start;

/**
 * @brief icmp_input - Processa e responde autonomamente a mensagens ICMP da Camada 3.
 */
int icmp_input(const void* data, uint32_t len, uint32_t src_ip, ip_header_t* ip_hdr)
{
    (void)ip_hdr; /* Evita aviso de variável não utilizada se não for necessária */

    /* 1. Validação elementar de integridade física do payload */
    if (!data || len < sizeof(icmp_header_t))
    {
        return -1; /* Pacote corrompido ou curto demais */
    }

    /* 2. Mapeia o cabeçalho ICMP sobre os dados recebidos */
    icmp_header_t* icmp = (icmp_header_t*)data;

    /* 3. Triagem de Tipo: Interessa-nos estritamente o Echo Request (Ping) */
    if (icmp->type == ICMP_TYPE_ECHO_REQUEST)
    {
        /* Extrai o número de sequência e o ID convertendo de Network Byte Order (Big-Endian) */
        uint16_t seq = ntohs(icmp->sequence);
        
        /* Extrai o TTL diretamente do cabeçalho IP passado por argumento */
        uint8_t ttl = ip_hdr->ttl;

        /* 1. Captura o TSC final imediatamente */
        uint64_t tsc_end = read_tsc();
        uint64_t cycles_elapsed = tsc_end - g_current_packet_tsc_start;

        /* 3. CALCULO DINÂMICO DO TEMPO:
         * Se a frequência do CPU não estiver mapeada, assumimos a lógica de microsegundos no QEMU.
         * Emuladores processam isto tão rápido que 'cycles_elapsed' dividido pela frequência costuma dar 0 ms.
         */
        uint64_t time_ms = 0;
        if (g_tsc_hz > 0)
        {
            /* Ciclos / (Frequência em MHz * 1000) = Tempo em milissegundos */
            time_ms = (uint32_t)(cycles_elapsed / (g_tsc_hz * 1000));
        }

        if (time_ms == 0)
            time_ms = 1;

        /* Formata o print canónico de rede na consola do Sirius OS */
        kprintf("[ICMP Input] %d bytes de %d.%d.%d.%d: icmp_seq=%d ttl=%d time=%d ms\n",
                len,
                (src_ip & 0xFF), 
                ((src_ip >> 8) & 0xFF),
                ((src_ip >> 16) & 0xFF), 
                ((src_ip >> 24) & 0xFF),
                seq, 
                ttl,
                time_ms);


        /* 
         * ALOCAÇÃO DINÂMICA COMPLETA DO BUFFER DE RESPOSTA:
         * O tamanho total do Reply deve ser rigorosamente igual ao tamanho recebido,
         * preservando os dados opcionais (payload) enviados pelo utilitário ping.
         */
        uint8_t* reply_buf = (uint8_t*)kmalloc(len);
        if (!reply_buf)
        {
            kprintf("[ICMP Error] Memória insuficiente para responder ao Ping.\n");
            return -2;
        }

        /* Copia integralmente o pacote original para manter os dados de payload intactos */
        memcpy(reply_buf, data, len);

        /* Mapeia a estrutura na zona alocada para modificar os campos necessários */
        icmp_header_t* reply_icmp = (icmp_header_t*)reply_buf;

        /* Transforma a mensagem em Echo Reply mantendo ID e Sequência intactos */
        reply_icmp->type = ICMP_TYPE_ECHO_REPLY;
        reply_icmp->code = 0;
        reply_icmp->checksum = 0; /* Zera para forçar o recálculo sobre o bloco completo */

        /* 
         * Recálculo do Checksum ICMP:
         * Abrange obrigatoriamente o cabeçalho ICMP E todo o seu payload de dados.
         */
        reply_icmp->checksum = ip_calculate_checksum(reply_buf, len);

        /* 
         * DEVOLUÇÃO EM CASCATA VIA IP_OUTPUT:
         * Devolve o pacote invertendo o fluxo, enviando-o de volta para o IP de origem.
         * O protocolo IP correspondente para o ICMP é o 1.
         */
        int res = ip_output(src_ip, IPPROTO_ICMP, reply_buf, len);

        kfree(reply_buf);
        return res;
    }
    
    /* Outros tipos de ICMP (Destination Unreachable, Time Exceeded) são descartados */
    return 0;
}