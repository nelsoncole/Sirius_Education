/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sshd.c
 *    Description: Daemon de acesso remoto seguro no Ring 3. Implementa o
 *                 Handshake e a gestão de sessão SSL/TLS utilizando a 
 *                 biblioteca unificada TLSe, mantendo a ponte de E/S com o
 *                 Kernel através de unistd.h.
 * 
 *         Author: Nelson Cole (Portabilidade e Adaptacao)
 *   Created Date: 01/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 01/10/2026
 * ============================================================================
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    int socket_desc, client_sock;
    unsigned int c;
    struct sockaddr_in server, client;
    
    /* 1. Inicialização do Socket nativo do Sirius OS */
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("[telnetd] Erro ao criar socket nativo.\n");
        return -1;
    }
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = 0;      /* INADDR_ANY: Escuta em todas as interfaces */
    server.sin_port = htons(443);    /* Mantemos na porta 443 para bater com o seu setup anterior */

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("[telnetd] Erro no Bind de porta.\n");
        return 1;
    }
     
    listen(socket_desc, 3);
    c = sizeof(struct sockaddr_in);

    while (1) {
        /* 2. O seu accept() POSIX corrigido colhe a sessão do Kernel */
        client_sock = accept(socket_desc, (struct sockaddr *)&client, &c);
        if (client_sock < 0) {
            continue;
        }
        /* 3. Lança a Shell ligada à rede */
        int pid = fork();
        if (pid == 0)
        {
            // Processo Filho: assume o terminal remoto
            close(socket_desc); // Fecha o socket pai neste contexto filho

            /* 
             * PONTE MÁGICA DO VFS:
             * Redireciona a Entrada, Saída e Erros padrão do processo para o FD da rede.
             * Tudo o que o sh.elf escrever irá direto para a placa de rede!
             */
            dup2(client_sock, 0); // stdin  <- Placa de Rede
            dup2(client_sock, 1); // stdout -> Placa de Rede
            dup2(client_sock, 2); // stderr -> Placa de Rede

            // Executa o interpretador de comandos nativo do Sirius OS
            char *sh_args[] = {"apps/bin/sh", NULL};
            execve(sh_args[0], sh_args, NULL);

            // Caso o execve falhe, encerra graciosamente
            _exit(-1);
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);

            // Processo Pai: Desconecta-se deste cliente e volta para o accept()
            close(client_sock); 
        }
    }
    
    return 0;
}