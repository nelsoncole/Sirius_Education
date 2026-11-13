#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <procyon/procyon.h>

#define MAX_TASKBAR_ICONS 32
#define ICON_WIDTH 50
#define ICON_HEIGHT 40
#define ICON_SPACING 8

int width, height;

static int startmenu_visible = 0;
window_t *menu_win = 0;

// Estrutura para gerir os ícones da barra de tarefas
typedef struct {
    ui_context_t *tui;
    component_t *icons[MAX_TASKBAR_ICONS];
    int count;
    int bar_width;
    int bar_height;
} taskbar_manager_t;

static taskbar_manager_t taskbar;
void run_start_menu_process(void);
void on_start_menu_click(void)
{
    startmenu_visible = !startmenu_visible;
    if (startmenu_visible)
    {
        pid_t pid = fork();
        if(pid == 0) run_start_menu_process();
    }
}

void on_dummy_click(void)
{
    // Ação de exemplo para outros ícones
}

// 1. Inicializa o gestor da barra de tarefas
void taskbar_init(ui_context_t *tui, int bar_width, int bar_height)
{
    taskbar.tui = tui;
    taskbar.count = 0;
    taskbar.bar_width = bar_width;
    taskbar.bar_height = bar_height;
}

// 2. Recalcula e centraliza todos os ícones registados
void taskbar_recalculate_layout(void)
{
    if (taskbar.count == 0) return;

    // Calcula a largura total ocupada pelo bloco de ícones
    int total_width = (taskbar.count * ICON_WIDTH) + ((taskbar.count - 1) * ICON_SPACING);
    
    // Calcula o ponto X inicial para centrar na barra
    int start_x = (taskbar.bar_width - total_width) / 2;
    int pos_y = (taskbar.bar_height - ICON_HEIGHT) / 2;

    for (int i = 0; i < taskbar.count; i++)
    {
        component_t *comp = taskbar.icons[i];
        
        // Atualiza as coordenadas do componente (ajuste os nomes dos campos se necessário)
        comp->x = start_x + (i * (ICON_WIDTH + ICON_SPACING));
        comp->y = pos_y;
    }
}

// 3. Adiciona um novo ícone e centraliza automaticamente
component_t* taskbar_add_icon(const char *label, const char *icon_path, void (*callback)(void))
{
    if (taskbar.count >= MAX_TASKBAR_ICONS) return NULL;

    // Cria o ícone inicialmente na posição (0,0)
    component_t *icon = create_iconbutton(taskbar.tui, 0, 0, ICON_WIDTH, ICON_HEIGHT, label, icon_path, 32, callback);
    if (!icon) return NULL;
    icon->bg = taskbar.tui->window->bg;
    iconbutton_data_t *b = (iconbutton_data_t *)icon->data;
    b->border_thickness = 0;
    b->border_radius = 0;
    b->show_label = 0;
    b->bg_normal = taskbar.tui->window->bg;

    // Regista o ícone e recalcula as posições de todos
    taskbar.icons[taskbar.count++] = icon;
    taskbar_recalculate_layout();

    return icon;
}

void procyon_desktop_main(int info_width, int info_height) {

    width = info_width;
    height = info_height;

    int socket_fd = procyon_init_client();
    if (socket_fd < 0) {
        exit(-1);
    }

    window_t* desktop_win = overlay_window_create(0, 0, width, height, 0xFF000000, 0x0000FF);
    if (!desktop_win) {
        procyon_finish_client(socket_fd);
        exit(-1);
    }

    image_t img;
    if (!load_image("usr/share/wallpaper/sirius.png", &img)) {
        unsigned char *img_data = draw_image_resize(img.pixels, img.width, img.height,
                                                    desktop_win->width, desktop_win->height);

        draw_image(0, 0, desktop_win->width, desktop_win->height,
                   img_data, desktop_win->width, desktop_win->height, desktop_win->buffer);

        image_free(&img);
        free(img_data);
    }

    ui_context_t ui = {.fd = socket_fd, .window = desktop_win };
    enviar_janela_ao_servidor(socket_fd, desktop_win, 0);

    int pid_taskbar = fork();
    if (pid_taskbar == 0)
    {
        int fd = procyon_init_client();
        if (fd < 0)
        {
            exit(-1);
        }

        int taskbar_height = 50;
        window_t *win = overlay_window_create(0, height - taskbar_height, width, taskbar_height, RGBA_TO_ARGB(rgba(245, 245, 255, 255)), 0xFF000000);
        if (!win)
        {
            procyon_finish_client(fd);
            exit(-2);
        }

        ui_context_t tui = {.fd = fd, .window = win };
        // Inicializa o gestor da barra de tarefas
        taskbar_init(&tui, width, taskbar_height);

        int menu_w = 480;
        int menu_h = 380;
        int taskbar_h = 50;

        int menu_x = (width - menu_w) / 2;
        int menu_y = height - taskbar_h - menu_h - 5;

        menu_win = overlay_window_create(menu_x, menu_y, menu_w, menu_h, RGBA_TO_ARGB(rgba(30, 35, 48, 190)), 0xFFFFFFFF);

        // Adiciona os ícones (cada chamada recalcula e centraliza o conjunto)
        taskbar_add_icon("Start", "usr/share/icons/menu.png", on_start_menu_click);
        taskbar_add_icon("Files", "usr/share/icons/folder.png", on_dummy_click);
        taskbar_add_icon("Terminal", "usr/share/icons/terminal.png", on_dummy_click);

        enviar_janela_ao_servidor(fd, win, 2);

        while (1)
        {
            ui_draw(&tui);
            event_t ev = get_next_event_window(win);
            ui_event(&tui, ev);
        }

        _exit(0);
    }

    while (1)
    {
        ui_draw(&ui);
        event_t ev = get_next_event_window(desktop_win);
        ui_event(&ui, ev);
    }

    procyon_finish_client(socket_fd);
    exit(0);
}


void run_start_menu_process(void)
{

    int menu_fd = procyon_init_client();
    if (menu_fd < 0) {
        _exit(-1);
    }

    if (!menu_win) {
        procyon_finish_client(menu_fd);
        _exit(-1);
    }

    ui_context_t mui = { .fd = menu_fd, .window = menu_win };

    enviar_janela_ao_servidor(menu_fd, menu_win, 2);

    while (1)
    {
        ui_draw(&mui);
        event_t ev = get_next_event_window(menu_win);
        ui_event(&mui, ev);

        if(ev.type == EVENT_FOCUS_LOST) {
            enviar_remocao_janela(menu_fd, menu_win);
            break;
        }
    }

    procyon_finish_client(menu_fd);
    _exit(0);
}
