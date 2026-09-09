/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mouse.c
 *    Description: Implementação do driver do rato PS/2 em modo Raw.
 *                 Gere a máquina de estados para agrupamento do pacote de
 *                 3 bytes, processa os sinais de complemento de dois e 
 *                 disponibiliza os dados para o subsistema gráfico.
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

#include <kernel/drivers/char/mouse.h>
#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/klib.h>

#define IRQ_MOUSE 12 /* O Rato PS/2 secundário está mapeado obrigatoriamente na IRQ 12 */

/* Máquina de estados para controlo sequencial dos bytes do rato */
static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_bytes[3];

/* Buffer circular para armazenamento dos pacotes de movimento montados */
#define MOUSE_BUFFER_SIZE 64
static mouse_packet_t g_mouse_buffer[MOUSE_BUFFER_SIZE];
static int g_mouse_head = 0;
static int g_mouse_tail = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * Função auxiliar bloqueante para aguardar que o buffer do 8042 fique pronto.
 * type: 0 = Aguarda dados para leitura, 1 = Aguarda libertação para escrita.
 */
static void mouse_wait(uint8_t type)
{
    uint32_t timeout = 100000;
    if (type == 0)
    {
        while (timeout--)
        {
            if ((inb(MOUSE_STATUS_PORT) & 1) == 1) return;
        }
    }
    else
    {
        while (timeout--)
        {
            if ((inb(MOUSE_STATUS_PORT) & 2) == 0) return;
        }
    }
}

/**
 * Escreve um comando direcionado estritamente ao chip do rato.
 */
static void mouse_write_cmd(uint8_t cmd)
{
    mouse_wait(1);
    outb(MOUSE_STATUS_PORT, MOUSE_CMD_WRITE_NEXT);
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, cmd);
}

void mouse_ps2_init(void)
{
    kprintf("[Rato] Inicializando barramento auxiliar PS/2 (IRQ %d)...\n", IRQ_MOUSE);

    g_mouse_cycle = 0;
    g_mouse_head = 0;
    g_mouse_tail = 0;

    /* 1. ATIVAÇÃO DA PORTA AUXILIAR NO CONTROLADOR 8042 */
    mouse_wait(1);
    outb(MOUSE_STATUS_PORT, 0xA8); /* Ativa a segunda porta PS/2 (Rato) */

    /* 2. LEITURA E ATIVAÇÃO DO COMPORTAMENTO DE INTERRUPÇÃO (Comand Byte) */
    mouse_wait(1);
    outb(MOUSE_STATUS_PORT, 0x20); /* Solicita a leitura do Command Byte atual */
    mouse_wait(0);
    uint8_t status = inb(MOUSE_DATA_PORT) | 2; /* Liga o Bit 1: Habilita IRQ 12 do rato */
    
    mouse_wait(1);
    outb(MOUSE_STATUS_PORT, 0x60); /* Informa que vai reescrever o Command Byte */
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, status);

    /* 3. ATIVAÇÃO DOS SINALIZADORES DE TRANSMISSÃO DO PRÓPRIO RATO */
    mouse_write_cmd(MOUSE_CMD_ENABLE_REPT);
    mouse_wait(0);
    inb(MOUSE_DATA_PORT); /* Consome o byte de ACK (0xFA) enviado pelo rato */

    /* 4. REGISTO DO HANDLER NA NOSSA KAPI MODULAR */
    kapi_register_irq_handler(IRQ_MOUSE, mouse_handler);
    
    kprintf("[Rato] Inicialização e roteamento concluídos com sucesso.\n");
}

/**
 * Rotina de interrupção ISR do Rato.
 * Agrupa sequencialmente os 3 bytes gerados por hardware.
 */
__attribute__((force_align_arg_pointer))
void mouse_handler(void)
{
    /* Garante defensivamente que o buffer do 8042 possui dados prontos para leitura */
    uint8_t status = inb(MOUSE_STATUS_PORT);
    if (!(status & 1)) return;
    
    /* PROTEÇÃO IMPORTANTE: Bit 5 (0x20) indica se o byte veio do rato ou do teclado */
    if (!(status & 0x20)) return; 

    uint8_t mouse_byte = inb(MOUSE_DATA_PORT);

    /* Sincronização de segurança: O primeiro byte do ciclo deve ter obrigatoriamente o Bit 3 ativo */
    if (g_mouse_cycle == 0 && !(mouse_byte & (1 << 3))) return;

    g_mouse_bytes[g_mouse_cycle++] = mouse_byte;

    /* Quando completamos o ciclo de 3 bytes, montamos o pacote estável */
    if (g_mouse_cycle == 3)
    {
        g_mouse_cycle = 0; /* Reseta a máquina de estados */

        mouse_packet_t packet;
        packet.buttons = g_mouse_bytes[0] & 0x07; /* Preserva apenas os bits 0, 1 e 2 (Botões) */

        /* Processamento do deslocamento X em complemento de dois */
        if (g_mouse_bytes[0] & MOUSE_X_SIGN)
        {
            packet.delta_x = (int16_t)(g_mouse_bytes[1] | 0xFF00); /* Extensão de sinal negativo */
        }
        else
        {
            packet.delta_x = (int16_t)g_mouse_bytes[1];
        }

        /* Processamento do deslocamento Y em complemento de dois (Invertido na especificação PS/2) */
        if (g_mouse_bytes[0] & MOUSE_Y_SIGN)
        {
            packet.delta_y = (int16_t)(g_mouse_bytes[2] | 0xFF00);
        }
        else
        {
            packet.delta_y = (int16_t)g_mouse_bytes[2];
        }
        
        /* Converte a orientação padrão invertida do hardware do Y para coordenadas normais de ecrã */
        packet.delta_y = -packet.delta_y;

        /* Enfileira o pacote estável montado no buffer circular */
        int next = (g_mouse_head + 1) % MOUSE_BUFFER_SIZE;
        if (next != g_mouse_tail)
        {
            g_mouse_buffer[g_mouse_head] = packet;
            g_mouse_head = next;
        }

        //kprintf("[%dx%d %2x] ",packet.delta_x, packet.delta_y, packet.buttons);
    }
}

/**
 * Retorna o pacote do rato do buffer de forma não-bloqueante.
 * @return 1 se um pacote válido foi capturado, 0 se o buffer estiver vazio.
 */
int mouse_get_packet(mouse_packet_t* packet)
{
    if (g_mouse_tail == g_mouse_head || !packet) return 0;

    *packet = g_mouse_buffer[g_mouse_tail];
    g_mouse_tail = (g_mouse_tail + 1) % MOUSE_BUFFER_SIZE;
    
    return 1;
}
