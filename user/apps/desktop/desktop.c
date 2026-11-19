#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <procyon/procyon.h>
#include <string.h>

#include "desktop.h"

extern char **environ;

#define MAX_TASKBAR_ICONS 32
#define ICON_WIDTH 50
#define ICON_HEIGHT 40
#define ICON_SPACING 8


#define MAX_START_APP_ICONS 32
#define START_APP_ICON_WIDTH 100
#define START_APP_ICON_HEIGHT 100
#define START_MENU_ICON_SPACING_X  12
#define START_MENU_ICON_SPACING_Y  16
#define START_MENU_START_X         25
#define START_MENU_START_Y         95


int start_all_apps, start_power_requested;


#define START_MENU_WIDTH (START_APP_ICON_WIDTH*5 + START_MENU_ICON_SPACING_X*5 + START_MENU_START_X*2)
#define START_MENU_HEIGHT 550

static int startmenu_visible = 0;
window_t *menu_win = 0;

static int width, height;
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
void on_start_menu_click(component_t *comp)
{
    (void)comp;

    startmenu_visible = !startmenu_visible;
    if (startmenu_visible)
    {
        pid_t pid = fork();
        if(pid == 0) run_start_menu_process();
    }
}

void on_dummy_click(component_t *comp)
{
    (void)comp;
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

void start_app_init(ui_context_t *ui)
{
    
    component_t *panel = create_panel(ui, 0, 0, START_MENU_WIDTH, START_MENU_START_Y - 10, RGBA_TO_ARGB(rgba(200, 200, 200, 255)), 4, 0xFFE0E0E0);
    
    create_editbox(ui, START_MENU_START_X, (START_MENU_START_Y - 40)/2, START_MENU_WIDTH-(START_MENU_START_X*2), 40);

    panel = create_panel(ui, 0, START_MENU_HEIGHT-(START_MENU_START_Y - 10), START_MENU_WIDTH, START_MENU_START_Y - 10, RGBA_TO_ARGB(rgba(200, 200, 200, 255)), 4, 0xFFE0E0E0);
    if (panel)
    {
        component_t *icon = create_iconbutton(ui, panel->x + START_MENU_START_X, panel->y + (panel->h-64)/2,
                                             48 + (strlen("Nelson Cole")*(18/2)), 64,
                                             "Nelson Cole", "usr/share/icons/profile.png", 48, on_dummy_click);

        if (icon) {
            icon->border_color = panel->bg;
            icon->fg = 0xFF000000;
            icon->bg = panel->bg;
            icon->font_size = 18;
            iconbutton_data_t *b = (iconbutton_data_t *)icon->data;
            b->bg_normal = panel->bg;
            b->layout = ICON_LABEL_RIGHT;
        }

        icon = create_iconbutton(ui, panel->x + (START_MENU_WIDTH - (START_MENU_START_X + START_MENU_ICON_SPACING_X + 64)), panel->y + (panel->h-64)/2,
                                             64, 64,"", "usr/share/icons/power.png", 38, on_dummy_click);

        if (icon) {
            icon->border_color = panel->bg;
            icon->fg = 0xFF000000;
            icon->bg = panel->bg;
            icon->font_size = 18;
            iconbutton_data_t *b = (iconbutton_data_t *)icon->data;
            b->bg_normal = panel->bg;
            b->show_label = 0;
            b->layout = ICON_CENTER;
        }
    }
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
component_t* taskbar_add_icon(const char *label, const char *icon_path, void (*callback)(component_t *comp))
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

void on_menu_click(component_t *comp)
{
    if(!comp || comp->command[0] == '\0') return;

    pid_t pid;
    int ret;

    char *argv[] = {
        comp->command,
        NULL
    };

    ret = posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
    if (ret != 0) {
        printf("[on_menu_click] Erro ao executar processo: %d\n", ret);
    }else {
        printf("[on_menu_click] Processo criado: PID %d\n", pid);
    }

}

static component_t* scroll_start_menu_add_app(component_t *scroll, int id, const char *name, const char *icon_path, char *command) {

    scroll_data_t *d = (scroll_data_t*)scroll->data;
    if(!d) return NULL;
   
    component_t *app = create_iconbutton(d->ctx, 0, 0,
        START_APP_ICON_WIDTH, START_APP_ICON_HEIGHT,
        name, icon_path, 48, on_menu_click);

    if (!app) return NULL;

    iconbutton_data_t *b = (iconbutton_data_t *)app->data;
    app->bg = d->ctx->window->bg;
    app->font_size = 18;
    app->border_color = app->bg;
    app->fg = 0xFF000000;
    b->bg_normal = app->bg;

    b->layout = ICON_LABEL_BOTTOM;

    app->id = id;

    strncpy(app->command, command, sizeof(app->command) - 1);
    app->command[sizeof(app->command) - 1] = '\0';

    scroll_add_component(scroll, app);

    return app;
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

    ui_context_t ui = {
        .fd = socket_fd, 
        .window = desktop_win,
        .components = NULL,
        .count = 0,
        .modal_active = 0,
        .modal_owner = NULL,
        .sub_modal_owner = NULL,
        .lock = 0
    };
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

        ui_context_t tui = {
            .fd = fd, 
            .window = win,
            .components = NULL,
            .count = 0,
            .modal_active = 0,
            .modal_owner = NULL,
            .sub_modal_owner = NULL,
            .lock = 0
        };
        // Inicializa o gestor da barra de tarefas
        taskbar_init(&tui, width, taskbar_height);

        int taskbar_h = 50;

        int menu_w = START_MENU_WIDTH;
        int menu_h = START_MENU_HEIGHT;

        int menu_x = (width - menu_w) / 2;
        int menu_y = height - taskbar_h - menu_h - 12;

        menu_win = overlay_window_create(
            menu_x,
            menu_y,
            menu_w,
            menu_h,
            RGBA_TO_ARGB(rgba(244, 244, 244, 255)),
            0xFF000000);

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

    if (menu_fd < 0 || !menu_win)
        _exit(-1);

    ui_context_t mui = {
        .fd = menu_fd,
        .window = menu_win,
        .components = NULL,
        .count = 0,
        .modal_active = 0,
        .modal_owner = NULL,
        .sub_modal_owner = NULL,
        .lock = 0
    };

    start_app_list_t app_list = {0};

    start_app_init(&mui);

    component_t *scroll = create_scroll(&mui, START_MENU_START_X, START_MENU_START_Y, START_MENU_WIDTH - (START_MENU_START_X * 2), START_MENU_HEIGHT - (START_MENU_START_Y * 2) - 100);

    scroll_set_layout(
        scroll, SCROLL_LAYOUT_GRID, START_MENU_ICON_SPACING_X,
        START_MENU_ICON_SPACING_Y, START_APP_ICON_WIDTH, START_APP_ICON_HEIGHT
    );

    if (start_menu_load_config("etc/start-menu.conf", &app_list) < 0)
    {
        printf("Erro ao carregar configuracao do Start Menu\n");
    }
    else{
        for (unsigned int i = 0; i < app_list.count; i++)
        {
            start_app_entry_t *app = &app_list.apps[i];

            scroll_start_menu_add_app(
                scroll,
                app->id,
                app->name,
                app->icon,
                app->command);
        }

        start_app_list_free(&app_list);
    }

    enviar_janela_ao_servidor(menu_fd, menu_win, 2);

    start_all_apps = 0;
    start_power_requested = 0;

    while (1)
    {
        ui_draw(&mui);

        event_t ev = get_next_event_window(menu_win);

        ui_event(&mui, ev);

        if (start_power_requested)
        {
            enviar_remocao_janela(menu_fd, menu_win);
            break;
        }

        if (ev.type == EVENT_FOCUS_LOST ||
            ev.type == EVENT_WINDOW_CLOSE)
        {
            enviar_remocao_janela(menu_fd, menu_win);
            break;
        }

        if (ev.type == EVENT_KEY_DOWN &&
            ev.key.keycode == KEY_ESCAPE)
        {
            enviar_remocao_janela(menu_fd, menu_win);
            break;
        }
    }

    procyon_finish_client(menu_fd);
    _exit(0);
}