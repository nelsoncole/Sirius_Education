
#include <procyon/procyon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define SHELL_PATH "usr/bin/sh"

typedef struct {
    int socket_fd;
    int master_fd;
    window_t *window;
    ui_context_t *ui;
    component_t *console;
    pthread_mutex_t lock;
    int running;
} terminal_context_t;


static int create_pty(int *master_fd, int *slave_fd)
{
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

    int flags = fcntl(*master_fd, F_GETFL, 0);

    if (flags >= 0)
        fcntl(*master_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;

error:
    close(*master_fd);
    *master_fd = -1;
    return -1;
}

static int start_shell(int master_fd, int slave_fd)
{
    int pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        close(master_fd);

        if (dup2(slave_fd, STDIN_FILENO) < 0)
            _exit(1);

        if (dup2(slave_fd, STDOUT_FILENO) < 0)
            _exit(1);

        if (dup2(slave_fd, STDERR_FILENO) < 0)
            _exit(1);

        if (slave_fd > STDERR_FILENO)
            close(slave_fd);

        char *args[] = {
            (char *)SHELL_PATH,
            NULL
        };

        char *env[] = {
            "TERM=xterm",
            "HOME=/",
            "PATH=/bin:/usr/bin",
            NULL
        };

        execve(SHELL_PATH, args, env);

        _exit(127);
    }

    close(slave_fd);

    return pid;
}

static void *terminal_reader(void *arg)
{
    terminal_context_t *terminal = arg;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        pthread_mutex_lock(&terminal->lock);
        int running = terminal->running;
        pthread_mutex_unlock(&terminal->lock);

        if (!running)
            break;

        int nbytes = read(
            terminal->master_fd,
            buffer,
            sizeof(buffer) - 1
        );

        if (nbytes > 0)
        {
            buffer[nbytes] = '\0';
            pthread_mutex_lock(&terminal->lock);

            console_write(terminal->console, buffer);
            ui_draw(terminal->console->ctx);
            pthread_mutex_unlock(&terminal->lock);

            continue;
        }

        if (nbytes < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            usleep(10000);
            continue;
        }

        if (nbytes < 0 && errno == EINTR)
            continue;

        break;
    }

    pthread_mutex_lock(&terminal->lock);
    terminal->running = 0;
    pthread_mutex_unlock(&terminal->lock);

    return NULL;
}

static void *terminal_event_thread(void *arg)
{
    terminal_context_t *terminal = arg;

    while (terminal->running)
    {
        event_t ev = get_next_event_window(terminal->window);

        if (!terminal->running)
            break;

        pthread_mutex_lock(&terminal->lock);

        ui_event(terminal->ui, ev);

        if(ev.type == EVENT_TEXT_INPUT){

            int c = ev.text.codepoint;     
            console_put_char(terminal->console, (char)c);
            write(terminal->master_fd, &c, 1);
        }
        else if(ev.type == EVENT_KEY_DOWN) {
            int c = ev.text.codepoint; 

            if(ev.text.codepoint == KEY_ENTER) {

                console_put_char(terminal->console, (char)c);
                write(terminal->master_fd, &c, 1);

            }else if(ev.text.codepoint == KEY_BACKSPACE) {

                console_put_char(terminal->console, (char)c);
                write(terminal->master_fd, &c, 1);
            }
        }
        else if (ev.type == EVENT_WINDOW_CLOSE)
        {
            terminal->running = 0;
            break;
        }

        ui_draw(terminal->console->ctx);
        pthread_mutex_unlock(&terminal->lock);
    }

    return NULL;
}

int main(void)
{
    int socket_fd = -1;
    int master_fd = -1;
    int slave_fd = -1;
    int pid_shell = -1;
    int thread_started = 0;

    pthread_t reader_thread;
    pthread_t event_thread;

    terminal_context_t terminal = {
        .socket_fd = -1,
        .master_fd = -1,
        .window = NULL,
        .ui = NULL,
        .console = NULL,
        .running = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER
    };

    socket_fd = procyon_init_client();

    if (socket_fd < 0)
        goto cleanup;

    window_t *win = window_create(
        30, 20,
        800, 600,
        "Console",
        argb(200, 0xCC, 0xCC, 0xCC),
        0xFF000000
    );

    if (!win)
        goto cleanup;

    ui_context_t ui = {
        .fd = socket_fd,
        .window = win,
        .components = NULL,
        .count = 0,
        .modal_active = 0,
        .modal_owner = NULL,
        .sub_modal_owner = NULL,
        .lock = 0
    };

    component_t *console = create_console(
        &ui,
        win->client.x,
        win->client.y,
        win->client.width,
        win->client.height
    );

    if (!console)
        goto cleanup;

    if (create_pty(&master_fd, &slave_fd) < 0)
        goto cleanup;

    pid_shell = start_shell(master_fd, slave_fd);
    slave_fd = -1;

    if (pid_shell < 0)
        goto cleanup;

    terminal.master_fd = master_fd;
    terminal.socket_fd = socket_fd;
    terminal.window = win;
    terminal.ui = &ui;
    terminal.console = console;
    terminal.running = 1;

    console_write(console, "Procyon Terminal\n");
    console_write(console, "Shell iniciado.\n\n");

    enviar_janela_ao_servidor(socket_fd, win, 1);

    if (pthread_create(
            &reader_thread,
            NULL,
            terminal_reader,
            &terminal) != 0)
    {
        terminal.running = 0;
        goto cleanup;
    }

    if (pthread_create(
            &event_thread,
            NULL,
            terminal_event_thread,
            &terminal) != 0)
    {
        terminal.running = 0;
        pthread_join(reader_thread, NULL);
        goto cleanup;
    }

    thread_started = 1;

    while (terminal.running)
    {
        pthread_mutex_lock(&terminal.lock);

        ui_draw(&ui);

        pthread_mutex_unlock(&terminal.lock);

        int status;

        if (waitpid(pid_shell, &status, WNOHANG) == pid_shell)
        {
            terminal.running = 0;
            break;
        }

        usleep(10000);
    }

    pthread_mutex_lock(&terminal.lock);
    terminal.running = 0;
    pthread_mutex_unlock(&terminal.lock);

    if (thread_started)
    {
        pthread_join(reader_thread, NULL);
        pthread_join(event_thread, NULL);
        thread_started = 0;
    }

    pthread_mutex_lock(&terminal.lock);
    console_write(console, "\nShell terminado.\n");
    ui_draw(&ui);
    pthread_mutex_unlock(&terminal.lock);

    if(enviar_remocao_janela(socket_fd, win) == 0){
        window_destroy(win);
    }

cleanup:

    if (thread_started)
    {
        pthread_mutex_lock(&terminal.lock);
        terminal.running = 0;
        pthread_mutex_unlock(&terminal.lock);

        pthread_join(reader_thread, NULL);
        pthread_join(event_thread, NULL);
    }

    if (slave_fd >= 0)
        close(slave_fd);

    if (master_fd >= 0)
        close(master_fd);

    if (pid_shell > 0)
    {
        kill(pid_shell, SIGTERM);
        waitpid(pid_shell, NULL, 0);
    }

    if (socket_fd >= 0)
        procyon_finish_client(socket_fd);

    pthread_mutex_destroy(&terminal.lock);

    return 0;
}