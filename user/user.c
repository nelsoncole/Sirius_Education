/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: user.c
 *    Description: Aplicação inicial de espaço de utilizador (Ring 3).
 *                 Emite "Hello Ring3!" via Assembly Inline autónomo.
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 15/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

/* Números lógicos das Syscalls definidos no vosso syscall.h */
#include "lib/usyscall.h"
#include "lib/uheap.h"

/**
 * Função autónoma de leitura em Ring 3 usando Assembly Inline.
 */
static inline unsigned long sys_read_inline(int fd, void* buffer, unsigned long size) {
    unsigned long ret;

    /*
     * CONVENÇÃO DE HARDWARE X86_64 PARA SYSCALL:
     * rax = Número da Syscall (SYS_READ = 0)
     * rdi = 1º Argumento (fd)
     * rsi = 2º Argumento (buffer)
     * rdx = 3º Argumento (size)
     */
    __asm__ __volatile__ (
        "syscall"
        : "=a"(ret)
        : "a"((unsigned long)SYS_READ), "D"((unsigned long)fd),
          "S"((unsigned long)buffer), "d"((unsigned long)size)
        : "rcx", "r11", "memory"
    );

    return ret;
}

/**
 * Função autónoma de escrita em Ring 3 usando Assembly Inline.
 */
static inline unsigned long sys_write_inline(int fd, const void* buffer, unsigned long size) {
    unsigned long ret;

    __asm__ __volatile__ (
        "syscall"
        : "=a"(ret)
        : "a"((unsigned long)SYS_WRITE), "D"((unsigned long)fd),
          "S"((unsigned long)buffer), "d"((unsigned long)size)
        : "rcx", "r11", "memory"
    );

    return ret;
}

unsigned long strlen(const char *s) {
    const char *tmp = s;
    while (*tmp != '\0') tmp++;
    return (unsigned long)(tmp - s);
}

void *memset(void *s, char val, size_t count)
{
	size_t i;
    unsigned char *tmp = (unsigned char *)s;
    for( i =0; i < count; i++) 
        *tmp++ = val;
    
    return s;
	
}

void *memcpy(void * restrict s1, const void * restrict s2, size_t n)
{	
	size_t p    = n;
	char *p_dest = (char*)s1;
	char *p_src  = (char*)s2;

	while(p--)
	*p_dest++ = *p_src++;
	return s1;
}
#define htons(v) ((((unsigned short)(v) & 0xFF00) >> 8) | (((unsigned short)(v) & 0x00FF) << 8))
struct in_addr {
    unsigned int s_addr;
};
struct sockaddr_in {
    unsigned short sin_family;   /* Família do endereço: Sempre AF_INET */
    unsigned short sin_port;     /* Porta de transporte (Network Byte Order) */
    struct in_addr sin_addr;     /* Endereço IPv4 de 32-bits (Network Byte Order) */
    unsigned char  sin_zero[8];  /* Preenchimento de alinhamento com struct sockaddr */
};

/**
 * @brief Associa o socket a uma porta local (ex: porta 68 para o cliente DHCP).
 */
int sys_bind(int sock_fd, const struct sockaddr_in* addr, unsigned int addrlen)
{
    return (int)syscall3(SYS_BIND, (uint64_t)sock_fd, (uint64_t)addr, (uint64_t)addrlen);
}

/**
 * @brief Bloqueia a execução à espera de um datagrama UDP vindo da e1000.
 */
int sys_recvfrom(int sock_fd, void* buffer, unsigned int len, unsigned int flags, 
                 struct sockaddr_in* src_addr, unsigned int* addrlen)
{
    return (int)syscall6(SYS_RECVFROM, 
                         (uint64_t)sock_fd, 
                         (uint64_t)buffer, 
                         (uint64_t)len, 
                         (uint64_t)flags, 
                         (uint64_t)src_addr, 
                         (uint64_t)addrlen);
}

int udp_send_test_message(const char* message, unsigned int msg_len)
{
    const char* ms1 = "[UDP CLIENT] A iniciar transmissao de teste...\n";
    sys_write_inline(1, ms1, strlen(ms1));

    /* 1. Criar o socket especificando o domínio AF_INET e tipo Datagrama (UDP) */
    // Nota: Substitua pelo nome exato da sua função interna de criação de sockets
    int sock_fd = syscall3(SYS_SOCKET, 2, 2, 17);
    if (sock_fd < 0)
    {
        const char* ms2 = "[UDP CLIENT] Erro: Falha ao criar o socket AF_INET.\n";
        sys_write_inline(1, ms2, strlen(ms2));
        return -1;
    }



    /* 2. Configurar o endereço de destino (IP: 10.225.63.146, Porta: 5000) */
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = 2;
    dest_addr.sin_port   = htons(5000); /* Porta convertida para Big-Endian */
    /* 
     * Montagem Cirúrgica do IP 10.225.63.146 em Network Byte Order (Big-Endian):
     * Byte 0: 10, Byte 1: 225, Byte 2: 63, Byte 3: 146
     */
    unsigned char ip_bytes[4] = {10, 225, 63, 146};
    memcpy(&dest_addr.sin_addr.s_addr, ip_bytes, 4);

    const char* ms3 = "[UDP CLIENT] A enviar x bytes para 10.225.63.146:5000...\n";
    sys_write_inline(1, ms3, strlen(ms3));

    /* 4. Disparar o envio através da API de Sockets */
    // Nota: Se a sua syscall aceitar a estrutura genérica, use (struct sockaddr*)&dest_addr
    int bytes_sent = syscall6(SYS_SENDTO, (unsigned long)sock_fd, (unsigned long)message, (unsigned long)msg_len, (unsigned long)0, 
                                (unsigned long)(const struct sockaddr_in*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent < 0)
    {
        const char* ms4 = "[UDP CLIENT] Erro de envio: Codigo \n";
        sys_write_inline(1, ms4, strlen(ms4));
        
        // Fallback Académico: Se a sua syscall 'sendto' ainda não estiver mapeada no VFS,
        // pode invocar diretamente a Camada 3 que corrigimos anteriormente:
        // int res = ip_output(dest_addr.sin_addr.s_addr, IPPROTO_UDP, message, msg_len);
        
        syscall1(SYS_CLOSE, sock_fd);
        return -2;
    }

    const char* ms5 = "[UDP CLIENT] Pacote entregue com sucesso ao barramento da e1000.\n";
    sys_write_inline(1, ms5, strlen(ms5));

    /* 3. Bloqueia à espera de dados (Por exemplo, pacotes do Servidor) */
    char rx_buffer[256];
    unsigned int len;
    int received = sys_recvfrom(sock_fd, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                                (struct sockaddr_in*)&dest_addr, &len);


    sys_write_inline(1, rx_buffer, received);
    /* 5. Encerrar o descritor e libertar recursos da tabela */
    syscall1(SYS_CLOSE, sock_fd);
    return 0;
}

/**
 * Ponto de entrada da aplicação Ring 3.
 */
int main(int argc, char* argv[]) {

    // Buffer local na Stack do Ring 3 para capturar a linha digitada
    char input_buffer[256];
    
    const char* prompt = "SiriusOS> ";
    sys_write_inline(1, prompt, strlen(prompt));

    /* 
     * Loop REPL (Read-Eval-Print Loop) básico controlado:
     * Aguarda por dados, lê a linha e imprime de volta em loop.
     */
    while (1) {
        // Bloqueia e aguarda dados do descritor 0 (Teclado/TTY)
        unsigned long bytes_lidos = sys_read_inline(0, input_buffer, sizeof(input_buffer) - 1);
        
        // Garante que, se dados válidos forem lidos, eles serão processados
        if (bytes_lidos > 0 && bytes_lidos != (unsigned long)-1) {
            
            // Imprime exatamente a quantidade de bytes recebida de volta na consola (fd 1)
            sys_write_inline(1, input_buffer, bytes_lidos);
        }

        // Reimprime o prompt do sistema para a próxima interação
        sys_write_inline(1, prompt, strlen(prompt));

        udp_send_test_message(input_buffer, bytes_lidos);
    }
    
    return 0; 
}