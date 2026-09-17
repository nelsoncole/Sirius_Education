/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: socket.h
 *    Description: Interface pública da API de Utilizador (UAPI) e estruturas
 *                 internas do Kernel para o subsistema de Sockets POSIX.
 *                 Suporta buffers circulares locais (AF_LOCAL) e
 *                 prepara o ecossistema para os drivers de rede.
 * 
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 17/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <kernel/lib/stdint.h>
#include <kernel/kernel/core/spinlock.h>

/* Famílias de Protocolos Fundamentais */
#define AF_UNSPEC   0
#define AF_LOCAL    1 /* Machine-local comms */
#define AF_INET     2 /* IPv4 */
#define PF_INET6    3 /* IPv6 */
#define PF_PACKET   4 /* Low level packet interface */

/* Tipos de Sockets POSIX */
#define SOCK_STREAM 1 /* Conexão orientada a fluxo (Fiável, TCP / AF_LOCAL stream) */
#define SOCK_DGRAM  2 /* Mensagens não orientadas a conexão (Não fiável, UDP) */

#define SHUT_RD   0 /* Desativa recepção (RX) */
#define SHUT_WR   1 /* Desativa transmissão (TX) */
#define SHUT_RDWR 2 /* Desativa ambas as direções */

/* Dimensão padrão do buffer circular de transmissão (16 KiB) */
#define SOCKET_BUFFER_SIZE  16384

struct socket; // Declaração antecipada

/**
 * @brief Tabela de operações internas para cada família de protocolo.
 */
typedef struct protocol_operations {
    int (*bind)(struct socket* sock, const void* addr, unsigned long addrlen);
    int (*connect)(struct socket* sock, const void* addr, unsigned long addrlen);
    long (*sendto)(struct socket* sock, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen);
    long (*recvfrom)(struct socket* sock, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen);
    int (*listen)(struct socket* sock, int backlog);
    struct socket* (*accept)(struct socket* sock);
} protocol_operations_t;

/**
 * @brief Estrutura de controlo física de um Socket no espaço de Kernel.
 */
typedef struct socket {
    int family;                     // AF_LOCAL, AF_INET, etc.
    int type;                       // SOCK_STREAM ou SOCK_DGRAM
    int state;                      // Estado interno do socket (0=Desconectado, 1=Conectado, 2=Listen)
    
    spinlock_t lock;

    uint8_t local_addr[256];        // Buffer genérico para guardar o endereço (IP ou caminho da string)
    unsigned long local_addr_len;   // Tamanho real do endereço guardado

    // PONTE POLIMÓRFICA DO PROTOCOLO (O Segredo da Modularidade!)
    protocol_operations_t* proto_ops;
    
    // Fila circular nativa de receção de dados (Ring Buffer)
    
    uint8_t* rx_buffer;  // Buffer de Entrada (Receção)
    uint32_t rx_head;
    uint32_t rx_tail;

    uint8_t* tx_buffer;  // Buffer de Saída (Transmissão)
    uint32_t tx_head;
    uint32_t tx_tail;
    
    // Pontes de ligação de arquitetura assíncrona
    struct socket* peer;            // Aponta para o socket parceiro conectado (Zero-Copy)
    struct socket* listen_queue;    // Cabeça da fila de conexões pendentes para o accept()
    struct socket* next;            // Próximo socket na fila de conexões pendentes
} socket_t;


/**
 * @brief Inicializa as estruturas globais e o subsistema de sockets do Kernel.
 */
void init_socket(void);

/**
 * @brief Cria um ponto de comunicação de rede ou local e devolve um File Descriptor.
 */
int socket(int family, int type, int protocol);

/**
 * @brief Associa um endereço local ou um caminho virtual VFS a um socket.
 */
int bind(int fd, const void* addr, unsigned long addrlen);

/**
 * @brief Coloca o socket em modo passivo, aguardando por conexões entrantes.
 */
int listen(int fd, int backlog);

/**
 * @brief Retira a primeira conexão da fila de pendentes e cria um novo socket conectado.
 */
int accept(int fd, void* addr, unsigned long* addrlen);

/**
 * @brief Inicia uma conexão ativa com um socket passivo remoto ou local.
 */
int connect(int fd, const void* addr, unsigned long addrlen);

/**
 * @brief Transmite uma mensagem para um socket específico através do seu endereço.
 */
long sendto(int fd, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen);

/**
 * @brief Recebe uma mensagem de um socket e armazena o endereço de origem.
 */
long recvfrom(int fd, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen);

long send(int fd, const void* buf, unsigned long len, int flags);

long recv(int fd, void* buf, unsigned long len, int flags);

int shutdown(int fd, int how);

#endif /* _SOCKET_H_ */