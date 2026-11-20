/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sshd.c
 *    Description: Daemon de acesso remoto seguro no Ring 3. Implementa o
 *                 Handshake, autenticação e gestão de sessões SSHv2 utilizando
 *                 a biblioteca libssh, mantendo a ponte de E/S com o Kernel
 *                 através de unistd.h e pseudo-terminais (PTY).
 *
 *                 Suporta múltiplas sessões simultâneas através de pthreads,
 *                 com sincronização individual de cada sessão.
 *
 *         Author: Nelson Cole
 *   Created Date: 01/10/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 20/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libssh/libssh.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>

#define BUFFER_SIZE 1024

typedef struct ssh_client_context
{
    ssh_session_t *session;

    int client_sock;
    int server_fd;
    int master_fd;
    int slave_fd;

    pid_t shell_pid;

    pthread_mutex_t lock;

    int running;
    int finished;

} ssh_client_context_t;

typedef struct ssh_session_slot
{
    pthread_t thread;
    ssh_client_context_t *ctx;
    int used;

} ssh_session_slot_t;

#define MAX_SESSIONS 32

static ssh_session_slot_t g_sessions[MAX_SESSIONS];
static pthread_mutex_t g_sessions_lock;

/* ============================================================================
 *                      Gestão de estado da sessão
 * ============================================================================ */

static int session_is_running(ssh_client_context_t *ctx)
{
    int running;

    pthread_mutex_lock(&ctx->lock);
    running = ctx->running;
    pthread_mutex_unlock(&ctx->lock);

    return running;
}

static void session_stop(ssh_client_context_t *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->lock);

    int was_running = ctx->running;
    ctx->running = 0;

    int client_sock = ctx->client_sock;
    pid_t shell_pid = ctx->shell_pid;

    pthread_mutex_unlock(&ctx->lock);

    if (!was_running)
        return;

    if (client_sock >= 0)
        shutdown(client_sock, SHUT_RDWR);

    if (shell_pid > 0)
        kill(shell_pid, SIGTERM);
}

/*
 * Recolhe sessões terminadas. O contexto só é libertado depois de
 * pthread_join() confirmar que a thread principal já não o utiliza.
 */
static void reap_finished_sessions(void)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        pthread_mutex_lock(&g_sessions_lock);

        ssh_session_slot_t *slot = &g_sessions[i];
        ssh_client_context_t *ctx = slot->ctx;

        if (!slot->used || !ctx)
        {
            pthread_mutex_unlock(&g_sessions_lock);
            continue;
        }

        pthread_mutex_lock(&ctx->lock);
        int finished = ctx->finished;
        pthread_mutex_unlock(&ctx->lock);

        if (!finished)
        {
            pthread_mutex_unlock(&g_sessions_lock);
            continue;
        }

        pthread_t thread = slot->thread;
        pthread_mutex_unlock(&g_sessions_lock);

        pthread_join(thread, NULL);

        pthread_mutex_lock(&g_sessions_lock);
        if (slot->used && slot->ctx == ctx)
        {
            slot->used = 0;
            slot->ctx = NULL;
        }
        pthread_mutex_unlock(&g_sessions_lock);

        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
    }
}

/* ============================================================================
 *                      Operações auxiliares de E/S
 * ============================================================================ */

static int write_all(int fd, const void *buffer, size_t size)
{
    const char *data = (const char *)buffer;
    size_t total = 0;

    while (total < size)
    {
        ssize_t n = write(fd, data + total, size - total);

        if (n > 0)
        {
            total += (size_t)n;
            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;

        return -1;
    }

    return (int)total;
}

static int ssh_write_all(ssh_session_t *session,
                         const void *buffer,
                         size_t size)
{
    const char *data = (const char *)buffer;
    size_t total = 0;

    while (total < size)
    {
        int n = ssh_channel_write(session,
                                  (char *)(data + total),
                                  (int)(size - total));

        if (n <= 0)
            return -1;

        total += (size_t)n;
    }

    return (int)total;
}

/* ============================================================================
 *                              Criação da PTY
 * ============================================================================ */

static int create_pty(int *master_fd, int *slave_fd)
{
    if (!master_fd || !slave_fd)
        return -1;

    *master_fd = -1;
    *slave_fd = -1;

    *master_fd = open("/dev/ptmx", O_RDWR);

    if (*master_fd < 0)
        return -1;

    unsigned int pty_id = 0;

    if (ioctl(*master_fd, TIOCGPTN, &pty_id) < 0)
        goto error;

    char slave_path[32];

    snprintf(slave_path, sizeof(slave_path),
             "/dev/pts/%u", pty_id);

    *slave_fd = open(slave_path, O_RDWR);

    if (*slave_fd < 0)
        goto error;

    struct termios term;

    if (tcgetattr(*slave_fd, &term) < 0)
        goto error;

    term.c_lflag |= ICANON;
    term.c_lflag &= ~(ECHO | ECHONL);

    if (tcsetattr(*slave_fd, TCSANOW, &term) < 0)
        goto error;

    return 0;

error:

    if (*slave_fd >= 0)
    {
        close(*slave_fd);
        *slave_fd = -1;
    }

    if (*master_fd >= 0)
    {
        close(*master_fd);
        *master_fd = -1;
    }

    return -1;
}

/* ============================================================================
 *                         Inicialização da Shell
 * ============================================================================ */

static pid_t start_shell(ssh_client_context_t *ctx)
{
    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        close(ctx->server_fd);
        close(ctx->master_fd);
        close(ctx->client_sock);

        dup2(ctx->slave_fd, STDIN_FILENO);
        dup2(ctx->slave_fd, STDOUT_FILENO);
        dup2(ctx->slave_fd, STDERR_FILENO);

        if (ctx->slave_fd > STDERR_FILENO)
            close(ctx->slave_fd);

        char *sh_args[] = {"usr/bin/sh", NULL};

        execve(sh_args[0], sh_args, NULL);

        _exit(127);
    }

    return pid;
}

/* ============================================================================
 *                  Thread: SSH -> PTY (Entrada da Shell)
 * ============================================================================ */

static void *ssh_to_pty_thread(void *arg)
{
    ssh_client_context_t *ctx = (ssh_client_context_t *)arg;

    char buffer[BUFFER_SIZE];

    while (session_is_running(ctx))
    {
        int nbytes = ssh_channel_read(ctx->session,
                                     buffer,
                                     sizeof(buffer),
                                     0);

        if (nbytes > 0)
        {
            if (write_all(ctx->master_fd, buffer, (size_t)nbytes) < 0)
                break;
        }
        else
        {
            break;
        }
    }

    session_stop(ctx);

    return NULL;
}

/* ============================================================================
 *                  Thread: PTY -> SSH (Saída da Shell)
 * ============================================================================ */

static void *loop_ponte_criptografica(void *arg)
{
    ssh_client_context_t *ctx = (ssh_client_context_t *)arg;

    char buffer[BUFFER_SIZE];

    while (session_is_running(ctx))
    {
        ssize_t nbytes = read(ctx->master_fd,
                              buffer,
                              sizeof(buffer));

        if (nbytes > 0)
        {
            if (ssh_write_all(ctx->session,
                              buffer,
                              (size_t)nbytes) < 0)
            {
                break;
            }
        }
        else
        {
            if (nbytes < 0 && errno == EINTR)
                continue;

            break;
        }
    }

    session_stop(ctx);

    return NULL;
}

/* ============================================================================
 *                    Gestão individual de uma sessão SSH
 * ============================================================================ */

static void *ssh_client_thread(void *arg)
{
    ssh_client_context_t *ctx = (ssh_client_context_t *)arg;

    ctx->session = malloc(sizeof(ssh_session_t));

    if (!ctx->session)
        goto cleanup;

    memset(ctx->session, 0, sizeof(ssh_session_t));

    ssh_init_session(ctx->session,
                     ctx->client_sock,
                     read,
                     write);

    if (ssh_do_banner_exchange(ctx->session) != 0)
        goto cleanup;

    /* 1. Processa o protocolo SSH até estabelecer a sessão */

    while (ctx->session->state != SSH_STATE_DISCONNECTED &&
           ctx->session->state != SSH_STATE_CONNECTED_SHELL)
    {
        if (ssh_process_packet(ctx->session) != 0)
            goto cleanup;
    }

    if (ctx->session->state != SSH_STATE_CONNECTED_SHELL)
        goto cleanup;

    printf("[sshd] Sessão SSH estabelecida. Criando PTY...\n");

    /* 2. Cria uma PTY exclusiva para esta sessão */

    if (create_pty(&ctx->master_fd, &ctx->slave_fd) < 0)
    {
        printf("[sshd] Erro ao criar PTY.\n");
        goto cleanup;
    }

    /* 3. Lança a Shell ligada à PTY */

    ctx->shell_pid = start_shell(ctx);

    if (ctx->shell_pid < 0)
    {
        printf("[sshd] Erro ao iniciar a shell.\n");
        goto cleanup;
    }

    close(ctx->slave_fd);
    ctx->slave_fd = -1;

    /* 4. Ativa as duas pontes de comunicação */

    pthread_t input_thread;
    pthread_t output_thread;

    int input_started = 0;
    int output_started = 0;

    if (pthread_create(&input_thread,
                       NULL,
                       ssh_to_pty_thread,
                       ctx) == 0)
    {
        input_started = 1;
    }
    else
    {
        session_stop(ctx);
        goto bridge_cleanup;
    }

    if (pthread_create(&output_thread,
                       NULL,
                       loop_ponte_criptografica,
                       ctx) == 0)
    {
        output_started = 1;
    }
    else
    {
        session_stop(ctx);
    }

    /* 5. Aguarda o encerramento das pontes */

    if (input_started)
        pthread_join(input_thread, NULL);

    if (output_started)
        pthread_join(output_thread, NULL);

bridge_cleanup:

    session_stop(ctx);

    if (ctx->shell_pid > 0)
    {
        int status;
        waitpid(ctx->shell_pid, &status, 0);
        ctx->shell_pid = -1;
    }

cleanup:

    if (ctx->slave_fd >= 0)
        close(ctx->slave_fd);

    if (ctx->master_fd >= 0)
        close(ctx->master_fd);

    if (ctx->session)
    {
        free(ctx->session);
        ctx->session = NULL;
    }

    if (ctx->client_sock >= 0)
        close(ctx->client_sock);

    pthread_mutex_lock(&ctx->lock);
    ctx->running = 0;
    ctx->finished = 1;
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

/* ============================================================================
 *                              Servidor SSH
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int socket_desc;
    struct sockaddr_in server;
    struct sockaddr_in client;

    memset(g_sessions, 0, sizeof(g_sessions));

    if (pthread_mutex_init(&g_sessions_lock, NULL) != 0)
    {
        printf("[ssh] Erro ao inicializar mutex das sessões.\n");
        return -1;
    }

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc < 0)
    {
        printf("[ssh] Erro ao criar socket nativo.\n");
        return -1;
    }

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(22);

    if (bind(socket_desc,
             (struct sockaddr *)&server,
             sizeof(server)) < 0)
    {
        printf("[ssh] Erro no Bind de porta.\n");
        close(socket_desc);
        return -1;
    }

    if (listen(socket_desc, 16) < 0)
    {
        printf("[ssh] Erro ao iniciar listen.\n");
        close(socket_desc);
        return -1;
    }

    printf("[sshd] Servidor SSH iniciado na porta 22.\n");

    while (1)
    {
        reap_finished_sessions();

        unsigned int client_size = sizeof(client);

        int client_sock = accept(socket_desc,
                                 (struct sockaddr *)&client,
                                 &client_size);

        if (client_sock < 0)
            continue;

        reap_finished_sessions();

        /* Cada cliente recebe uma estrutura independente */

        ssh_client_context_t *ctx =
            (ssh_client_context_t *)malloc(sizeof(ssh_client_context_t));

        if (!ctx)
        {
            close(client_sock);
            continue;
        }

        memset(ctx, 0, sizeof(ssh_client_context_t));

        ctx->client_sock = client_sock;
        ctx->server_fd = socket_desc;
        ctx->master_fd = -1;
        ctx->slave_fd = -1;
        ctx->shell_pid = -1;
        ctx->running = 1;
        ctx->finished = 0;

        if (pthread_mutex_init(&ctx->lock, NULL) != 0)
        {
            close(client_sock);
            free(ctx);
            continue;
        }

        int slot_index = -1;

        pthread_mutex_lock(&g_sessions_lock);

        for (int i = 0; i < MAX_SESSIONS; i++)
        {
            if (!g_sessions[i].used)
            {
                slot_index = i;
                break;
            }
        }

        if (slot_index >= 0)
        {
            ssh_session_slot_t *slot = &g_sessions[slot_index];
            slot->ctx = ctx;
            slot->used = 1;

            if (pthread_create(&slot->thread,
                               NULL,
                               ssh_client_thread,
                               ctx) != 0)
            {
                slot->ctx = NULL;
                slot->used = 0;
                slot_index = -1;
            }
        }

        pthread_mutex_unlock(&g_sessions_lock);

        if (slot_index < 0)
        {
            printf("[sshd] Limite de sessões atingido ou erro ao criar thread.\n");
            pthread_mutex_destroy(&ctx->lock);
            close(client_sock);
            free(ctx);
            continue;
        }

        printf("[sshd] Nova sessão aceite. Slot: %d\n", slot_index);
    }

    close(socket_desc);

    return 0;
}