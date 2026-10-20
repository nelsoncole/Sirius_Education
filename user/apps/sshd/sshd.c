/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sshd.c
 *    Description: Daemon de acesso remoto seguro no Ring 3. Implementa o
 *                 Handshake, autenticação e gestão de sessão SSHv2 utilizando
 *                 a biblioteca libssh, mantendo a ponte de E/S com o Kernel
 *                 através de unistd.h e pseudo-terminais (PTY).
 * 
 *         Author: Nelson Cole
 *   Created Date: 01/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 17/10/2026
 *
 *        License: MIT
 * ============================================================================
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <libssh/libssh.h>
#include <string.h>

#define BUFFER_SIZE 1024

void loop_ponte_criptografica(ssh_session_t *session, int master_fd);
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    char buffer[BUFFER_SIZE];
    int nbytes;

    int socket_desc, client_sock;
    unsigned int c;
    struct sockaddr_in server, client;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("[ssh] Erro ao criar socket nativo.\n");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = 0; /* INADDR_ANY: Escuta em todas as interfaces */
    server.sin_port = htons(22);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("[ssh] Erro no Bind de porta.\n");
        return 1;
    }

    listen(socket_desc, 3);
    c = sizeof(struct sockaddr_in);

    while (1)
    {
        client_sock = accept(socket_desc, (struct sockaddr *)&client, &c);
        if (client_sock < 0)
        {
            continue;
        }

        ssh_session_t *session = (ssh_session_t *)mmap(
            NULL, sizeof(ssh_session_t),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS,
            -1, 0);

        if (session == MAP_FAILED)
        {
            printf("[sshd] Erro catastrófico ao alocar memória partilhada via mmap.\n");
            return -1;
        }

        ssh_init_session(session, client_sock, read, write);

        if (ssh_do_banner_exchange(session) == 0)
        {
            while (session->state != SSH_STATE_DISCONNECTED)
            {
                if (ssh_process_packet(session) != 0)
                {
                    break;
                }

                if (session->state == SSH_STATE_CONNECTED_SHELL)
                {
                    printf("[sshd] Conexão SSH estabelecida. Alocando PTY...\n");

                    int master_fd = open("/dev/ptmx", O_RDWR);
                    if (master_fd < 0) {
                        printf("[sshd] Erro ao abrir /dev/ptmx\n");
                        break;
                    }

                    unsigned int pty_id = 0;
                    if (ioctl(master_fd, TIOCGPTN, &pty_id) < 0) {
                        printf("[sshd] Erro ao obter o ID dinâmico do PTY via ioctl.\n");
                        close(master_fd);
                        break;
                    }

                    char slave_path[32];
                    snprintf(slave_path, sizeof(slave_path), "/dev/pts/%u", pty_id);
                    printf("[sshd] PTY Alocado Dinamicamente: %s\n", slave_path);

                    int slave_fd = open(slave_path, O_RDWR);
                    if (slave_fd < 0) {
                        printf("[sshd] Erro ao abrir réplica dinâmica %s\n", slave_path);
                        close(master_fd);
                        break;
                    }

                    /* 3. Lança a Shell ligada à rede */
                    int pid_shell = fork();
                    if (pid_shell == 0)
                    {
                        close(socket_desc);
                        close(master_fd);

                        /*
                         * PONTE MÁGICA DO VFS:
                         * Redireciona a Entrada, Saída e Erros para a réplica PTY /dev/pts/x.
                         * O sh.elf agora comunicará em texto limpo com o Kernel.
                         */
                        dup2(slave_fd, 0); // stdin  <- PTY Réplica
                        dup2(slave_fd, 1); // stdout -> PTY Réplica
                        dup2(slave_fd, 2); // stderr -> PTY Réplica

                        if (slave_fd > 2)
                            close(slave_fd);

                        close(client_sock);

                        // Executa o interpretador de comandos nativo do Sirius OS
                        char *sh_args[] = {"apps/bin/sh", NULL};
                        execve(sh_args[0], sh_args, NULL);

                        // Caso o execve falhe, encerra graciosamente
                        _exit(-1);
                    }

                    close(slave_fd);

                    int pid_ponte = fork();
                    if (pid_ponte == 0)
                    {
                        while (session->state == SSH_STATE_CONNECTED_SHELL)
                        {
                            nbytes = ssh_channel_read(session, buffer, sizeof(buffer), 0);
                            if (nbytes > 0)
                            {
                                write(master_fd, buffer, nbytes);
                            }
                        }

                        close(master_fd);
                        _exit(0);
                    }

                    /* Entra no loop gerenciando a criptografia e o Mestre */
                    loop_ponte_criptografica(session, master_fd);

                    /* Limpeza após a sessão cair (ex: utilizador digitou 'exit') */
                    //kill(pid_ponte, 9); // Encerra o processo irmão de rede
                    //kill(pid_shell, 9); // Garante que a shell associada morre
                    waitpid(pid_shell, NULL, 0);
                    waitpid(pid_ponte, NULL, 0);

                    close(master_fd);

                    break;
                }
            }
        }
        close(client_sock);
    }

    return 0;
}

void loop_ponte_criptografica(ssh_session_t *session, int master_fd)
{
    char buffer[BUFFER_SIZE];
    int nbytes;

    while (session->state == SSH_STATE_CONNECTED_SHELL)
    {
        nbytes = read(master_fd, buffer, sizeof(buffer)); 
        if (nbytes > 0)
        {
            ssh_channel_write(session, buffer, nbytes);
        }
    }
}