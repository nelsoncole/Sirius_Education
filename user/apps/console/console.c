#include <procyon/procyon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>

#include "ansi_parser.h"

#define BUFFER_SIZE 1024
#define COMMAND_HISTORY_SIZE 32
#define SHELL_PATH "usr/bin/sh"

typedef struct {
    int socket_fd;
    int master_fd;
    window_t *window;
    ui_context_t *ui;
    component_t *console;
    pthread_mutex_t lock;
    int running;

    char input_buffer[BUFFER_SIZE];
    size_t input_length;

    char history[COMMAND_HISTORY_SIZE][BUFFER_SIZE];
    size_t history_count;
    size_t history_next;
    int history_position;
    char history_saved[BUFFER_SIZE];

    ansi_parser_t ansi;
    
} terminal_context_t;

static void ansi_parser_process(terminal_context_t *terminal, unsigned char c);
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

    struct termios term;
    
    if (tcgetattr(*slave_fd, &term) == 0) {
        term.c_lflag |= ICANON;
        term.c_lflag &= ~(ECHO | ECHONL);
        tcsetattr(*slave_fd, TCSANOW, &term);
    }

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

    if (pid == 0) {
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

static int terminal_write_shell(terminal_context_t *terminal,
                                const char *data,
                                size_t length)
{
    size_t written = 0;

    while (written < length) {
        ssize_t n = write(terminal->master_fd,
                          data + written,
                          length - written);

        if (n > 0) {
            written += (size_t)n;
            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(1000);
            continue;
        }

        return -1;
    }

    return 0;
}

static void terminal_redraw_input(terminal_context_t *terminal,
                                  const char *new_text)
{
    for (size_t i = 0; i < terminal->input_length; i++)
        console_put_char(terminal->console, '\b');

    terminal->input_length = 0;
    terminal->input_buffer[0] = '\0';

    if (!new_text)
        return;

    size_t length = strlen(new_text);

    if (length >= sizeof(terminal->input_buffer))
        length = sizeof(terminal->input_buffer) - 1;

    memcpy(terminal->input_buffer, new_text, length);
    terminal->input_buffer[length] = '\0';
    terminal->input_length = length;

    for (size_t i = 0; i < length; i++)
        console_put_char(terminal->console, terminal->input_buffer[i]);
}

static void terminal_history_add(terminal_context_t *terminal,
                                const char *command)
{
    if (!command || !command[0])
        return;

    if (terminal->history_count > 0) {
        size_t last = (terminal->history_next +
                       COMMAND_HISTORY_SIZE - 1) %
                      COMMAND_HISTORY_SIZE;

        if (strcmp(terminal->history[last], command) == 0)
            return;
    }

    strncpy(terminal->history[terminal->history_next],
            command,
            BUFFER_SIZE - 1);

    terminal->history[terminal->history_next][BUFFER_SIZE - 1] = '\0';

    terminal->history_next =
        (terminal->history_next + 1) % COMMAND_HISTORY_SIZE;

    if (terminal->history_count < COMMAND_HISTORY_SIZE)
        terminal->history_count++;
}

static void terminal_history_up(terminal_context_t *terminal)
{
    if (terminal->history_count == 0)
        return;

    if (terminal->history_position == -1) {
        memcpy(terminal->history_saved,
               terminal->input_buffer,
               terminal->input_length + 1);

        terminal->history_position = 0;
    } else if ((size_t)(terminal->history_position + 1) >=
               terminal->history_count) {
        return;
    } else {
        terminal->history_position++;
    }

    size_t index = (terminal->history_next +
                    COMMAND_HISTORY_SIZE - 1 -
                    (size_t)terminal->history_position) %
                   COMMAND_HISTORY_SIZE;

    terminal_redraw_input(terminal, terminal->history[index]);
}

static void terminal_history_down(terminal_context_t *terminal)
{
    if (terminal->history_position == -1)
        return;

    if (terminal->history_position == 0) {
        terminal->history_position = -1;
        terminal_redraw_input(terminal, terminal->history_saved);
        return;
    }

    terminal->history_position--;

    size_t index = (terminal->history_next +
                    COMMAND_HISTORY_SIZE - 1 -
                    (size_t)terminal->history_position) %
                   COMMAND_HISTORY_SIZE;

    terminal_redraw_input(terminal, terminal->history[index]);
}

static void terminal_submit_line(terminal_context_t *terminal)
{
    terminal->input_buffer[terminal->input_length] = '\0';

    terminal_history_add(terminal, terminal->input_buffer);

    console_put_char(terminal->console, '\n');

    if (terminal_write_shell(terminal,
                             terminal->input_buffer,
                             terminal->input_length) < 0) {
        console_write(terminal->console, "Erro ao enviar comando.\n");
    } else {
        const char newline = '\n';

        if (terminal_write_shell(terminal, &newline, 1) < 0)
            console_write(terminal->console, "Erro ao enviar ENTER.\n");
    }

    terminal->input_length = 0;
    terminal->input_buffer[0] = '\0';
    terminal->history_position = -1;
    terminal->history_saved[0] = '\0';
}

static void terminal_handle_text(terminal_context_t *terminal,
                                 uint32_t codepoint)
{
    if (codepoint < 32 || codepoint > 126)
        return;

    if (terminal->input_length >= sizeof(terminal->input_buffer) - 1)
        return;

    char c = (char)codepoint;

    terminal->input_buffer[terminal->input_length++] = c;
    terminal->input_buffer[terminal->input_length] = '\0';

    console_put_char(terminal->console, c);
    terminal->history_position = -1;
}

static void terminal_handle_backspace(terminal_context_t *terminal)
{
    if (terminal->input_length == 0)
        return;

    terminal->input_length--;
    terminal->input_buffer[terminal->input_length] = '\0';

    console_put_char(terminal->console, '\b');
    terminal->history_position = -1;
}

static void *terminal_reader(void *arg)
{
    terminal_context_t *terminal = arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        pthread_mutex_lock(&terminal->lock);
        int running = terminal->running;
        pthread_mutex_unlock(&terminal->lock);

        if (!running)
            break;

        int nbytes = read(
            terminal->master_fd,
            buffer,
            sizeof(buffer)
        );

        if (nbytes > 0) {
            pthread_mutex_lock(&terminal->lock);

            for (int i = 0; i < nbytes; i++) {
                ansi_parser_process(
                    terminal,
                    (unsigned char)buffer[i]
                );
            }

            ui_draw(terminal->console->ctx);

            pthread_mutex_unlock(&terminal->lock);
            continue;
        }

        if (nbytes < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)) {
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

    while (terminal->running) {
        event_t ev = get_next_event_window(terminal->window);

        if (!terminal->running)
            break;

        pthread_mutex_lock(&terminal->lock);

        ui_event(terminal->ui, ev);

        if (ev.type == EVENT_TEXT_INPUT) {
            terminal_handle_text(terminal, ev.text.codepoint);
        }
        else if (ev.type == EVENT_KEY_DOWN) {
            int key = ev.key.keycode;

            if (key == KEY_ENTER) {
                terminal_submit_line(terminal);
            }
            else if (key == KEY_BACKSPACE) {
                terminal_handle_backspace(terminal);
            }
            else if (key == KEY_UP) {
                terminal_history_up(terminal);
            }
            else if (key == KEY_DOWN) {
                terminal_history_down(terminal);
            }
        }
        else if (ev.type == EVENT_WINDOW_CLOSE) {
            terminal->running = 0;
            pthread_mutex_unlock(&terminal->lock);
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
    int reader_started = 0;
    int event_started = 0;

    pthread_t reader_thread;
    pthread_t event_thread;

    terminal_context_t terminal = {
        .socket_fd = -1,
        .master_fd = -1,
        .window = NULL,
        .ui = NULL,
        .console = NULL,
        .running = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .input_length = 0,
        .history_count = 0,
        .history_next = 0,
        .history_position = -1,
        .ansi = {0}
    };

    ansi_parser_init(&terminal.ansi);

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

    if (pthread_create(&reader_thread, NULL,
                       terminal_reader, &terminal) != 0) {
        terminal.running = 0;
        goto cleanup;
    }

    reader_started = 1;

    if (pthread_create(&event_thread, NULL,
                       terminal_event_thread, &terminal) != 0) {
        terminal.running = 0;
        goto cleanup;
    }

    event_started = 1;

    while (terminal.running) {
        pthread_mutex_lock(&terminal.lock);
        ui_draw(&ui);
        pthread_mutex_unlock(&terminal.lock);

        int status;

        if (waitpid(pid_shell, &status, WNOHANG) == pid_shell) {
            terminal.running = 0;
            break;
        }

        usleep(10000);
    }

    pthread_mutex_lock(&terminal.lock);
    terminal.running = 0;
    pthread_mutex_unlock(&terminal.lock);

    if (reader_started) {
        pthread_join(reader_thread, NULL);
        reader_started = 0;
    }

    if (event_started) {
        pthread_join(event_thread, NULL);
        event_started = 0;
    }

    pthread_mutex_lock(&terminal.lock);
    console_write(console, "\nShell terminado.\n");
    ui_draw(&ui);
    pthread_mutex_unlock(&terminal.lock);

    if (enviar_remocao_janela(socket_fd, win) == 0)
        window_destroy(win);

cleanup:
    pthread_mutex_lock(&terminal.lock);
    terminal.running = 0;
    pthread_mutex_unlock(&terminal.lock);

    if (reader_started)
        pthread_join(reader_thread, NULL);

    if (event_started)
        pthread_join(event_thread, NULL);

    if (slave_fd >= 0)
        close(slave_fd);

    if (master_fd >= 0)
        close(master_fd);

    if (pid_shell > 0) {
        kill(pid_shell, SIGTERM);
        waitpid(pid_shell, NULL, 0);
    }

    if (socket_fd >= 0)
        procyon_finish_client(socket_fd);

    pthread_mutex_destroy(&terminal.lock);

    return 0;
}


static 
void ansi_parser_process(terminal_context_t *terminal, unsigned char c)
{
    ansi_parser_t *p = &terminal->ansi;

    switch (p->state) {
        case ANSI_NORMAL:
            if (c == 0x1B) {
                p->state = ANSI_ESC;
            } else {
                console_put_char(terminal->console, c);
            }
            break;

        case ANSI_ESC:
            if (c == '[') {
                p->state = ANSI_CSI;
                p->param_count = 0;
                p->current_param = 0;
                p->has_param = 0;
            } else {
                p->state = ANSI_NORMAL;
            }
            break;

        case ANSI_CSI:
            if (c >= '0' && c <= '9') {
                p->current_param =
                    p->current_param * 10 + (c - '0');
                p->has_param = 1;
            }
            else if (c == ';') {
                if (p->param_count < 16) {
                    p->params[p->param_count++] =
                        p->has_param ? p->current_param : 0;
                }

                p->current_param = 0;
                p->has_param = 0;
            }
            else if (c >= 0x40 && c <= 0x7E) {
                if (p->param_count < 16) {
                    p->params[p->param_count++] =
                        p->has_param ? p->current_param : 0;
                }

                if (c == 'J' && p->params[0] == 2) {
                    console_clear(terminal->console);
                }

                p->state = ANSI_NORMAL;
            }
            else {
                p->state = ANSI_NORMAL;
            }
            break;
    }
}