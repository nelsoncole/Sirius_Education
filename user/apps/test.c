#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#define SOCKET_PATH "/tmp/sirius_socket"

int main(void)
{
    int server_fd;
    int client_fd;
    pid_t pid;

    struct sockaddr_un addr;

    //unlink(SOCKET_PATH);

    server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (bind(server_fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        close(server_fd);
        //unlink(SOCKET_PATH);
        return 1;
    }

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        close(server_fd);
        //unlink(SOCKET_PATH);
        return 1;
    }

    /*
     * ============================================================
     * CLIENTE
     * ============================================================
     */

    if (pid == 0)
    {
        int client_socket;
        char buffer[256];

        //sleep(1);
        

        client_socket = socket(AF_LOCAL, SOCK_STREAM, 0);

        if (client_socket < 0)
        {
            perror("client socket");
            exit(1);
        }

        if (connect(client_socket,
                    (struct sockaddr *)&addr,
                    sizeof(addr)) < 0)
        {
            perror("connect");
            close(client_socket);
            exit(1);
        }

        const char *message = "Ola servidor! Sou o cliente.";

        write(client_socket,
              message,
              strlen(message));

        memset(buffer, 0, sizeof(buffer));

        read(client_socket,
             buffer,
             sizeof(buffer) - 1);

        printf("[CLIENTE] Resposta: %s\n", buffer);

        close(client_socket);

        exit(0);
    }

    /*
     * ============================================================
     * SERVIDOR
     * ============================================================
     */

    printf("[SERVIDOR] Aguardando cliente...\n");

    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0)
    {
        perror("accept");
        close(server_fd);
        //unlink(SOCKET_PATH);
        return 1;
    }

    char buffer[256];

    memset(buffer, 0, sizeof(buffer));

    read(client_fd,
         buffer,
         sizeof(buffer) - 1);

    printf("[SERVIDOR] Recebeu: %s\n", buffer);

    const char *response = "Ola cliente! Mensagem recebida.";

    write(client_fd,
          response,
          strlen(response));

    close(client_fd);

    wait(NULL);

    close(server_fd);

    //unlink(SOCKET_PATH);

    return 0;
}