/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: dhcp_init.c
 *    Description: Inicializador do cliente DHCP do Kernel. Cria o socket UDP
 *                 canónico na porta 68 e dispara o pacote DHCP DISCOVER.
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 20/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/klib.h>


/* Protótipos externos das tuas funções nativas de Sockets do Kernel */
extern int       dhcp_send_discover(socket_t* sock);

/**
 * @brief net_init_dhcp_client - Inicializa o subsistema cliente DHCP do Kernel.
 *                               Cria o socket de controlo e faz o bind na porta 68.
 * 
 * @return 0 em caso de sucesso absoluto, ou valor negativo em caso de falha.
 */
int net_init_dhcp_client(void)
{
    kprintf("[DHCP CORE]: A preparar socket cliente UDP na porta 68...\n");

    /* 
     * 1. Cria o objeto socket físico no espaço de Kernel.
     * Usamos AF_INET (IPv4) e SOCK_DGRAM (UDP) conforme definido no teu socket.h.
     */
    socket_t* dhcp_socket = socket_create(AF_INET, SOCK_DGRAM, 0);
    if (!dhcp_socket)
    {
        kprintf("[DHCP CORE]: Erro: Falha catastrófica ao alocar socket_t.\n");
        return -1;
    }

    /* 2. Prepara a estrutura de endereço local para o Bind */
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons(68); /* Porta cliente padrão do DHCP */
    local_addr.sin_addr.s_addr   = 0;  /* INADDR_ANY (0.0.0.0) - Ainda não temos IP */
    memset(local_addr.sin_zero, 0, 8);

    /* 
     * 3. Invoca a operação polimórfica de bind associada ao protocolo.
     * O teu subsistema usa a proto_ops amarrada no socket_create.
     */
    if (dhcp_socket->proto_ops && dhcp_socket->proto_ops->bind)
    {
        int res = dhcp_socket->proto_ops->bind(dhcp_socket, &local_addr, sizeof(struct sockaddr_in));
        if (res != 0)
        {
            kprintf("[DHCP CORE]: Erro: Porta 68 ocupada ou recusada pelo protocolo.\n");
            return -2;
        }
    }
    else 
    {
        kprintf("[DHCP CORE]: Erro: Interface polimorfica proto_ops->bind ausente.\n");
        return -3;
    }

    /* 
     * 4. Dispara o primeiro passo do protocolo.
     * Envia o pacote DHCP DISCOVER em Broadcast completo via driver e1000 ativo.
     */
    int res = dhcp_send_discover(dhcp_socket);
    if (res != 0)
    {
        kprintf("[DHCP CORE]: Erro ao injetar DHCP DISCOVER no barramento.\n");
        return -4;
    }

    kprintf("[DHCP CORE]: Solicitacao enviada com sucesso! Interface aguardando DHCP OFFER.\n");
    
    /*
     * Nelson, o socket e fechado automaticamente pelo DHCP ACK 
     */
    return 0;
}