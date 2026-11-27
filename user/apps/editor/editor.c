
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <procyon/procyon.h>

#define MAX_MENU_BUTTON 8
#define MENU_BUTTON_HEIGHT 24
#define MENU_FONT_SIZE 18
#define MAX_FILE_PATH 512

window_t *win;
component_t *editor;

char current_file[MAX_FILE_PATH] = {0};
int document_open = 0;

typedef struct {
    ui_context_t *ui;
    component_t *btn[MAX_MENU_BUTTON];
    int count;
    int start_x;
    int start_y;
    int spacing_x;
} menu_manager_t;

menu_manager_t menu;

static void menu_recalculate_layout(void)
{
    if (menu.count <= 0)
        return;

    int x = menu.start_x;

    for (int i = 0; i < menu.count; i++) {
        component_t *btn = menu.btn[i];

        if (!btn)
            continue;

        btn->x = x;
        btn->y = menu.start_y;

        x += btn->w + menu.spacing_x;
    }
}

static component_t *menu_add_button(
    const char *label,
    void (*callback)(component_t *comp)
)
{
    if (!menu.ui || !menu.ui->window || !label ||
        menu.count >= MAX_MENU_BUTTON)
        return NULL;

    int width = (int)strlen(label) * (MENU_FONT_SIZE / 2) + 18;

    component_t *btn = create_button(
        menu.ui,
        0,
        0,
        width,
        MENU_BUTTON_HEIGHT,
        label,
        callback
    );

    if (!btn)
        return NULL;

    btn->font_size = MENU_FONT_SIZE;
    btn->bg = menu.ui->window->bg;

    button_data_t *b = (button_data_t *)btn->data;

    b->bg_normal = menu.ui->window->bg;
    b->border_thickness = 0;
    b->transparent_bg = 1;

    menu.btn[menu.count++] = btn;

    menu_recalculate_layout();

    return btn;
}

static void editor_set_current_file(const char *path)
{
    if (!path)
        return;

    strncpy(current_file, path, sizeof(current_file) - 1);
    current_file[sizeof(current_file) - 1] = '\0';
}

static void editor_clear_document(void)
{
    if (!editor)
        return;

    editbox_set_text(editor, "");

    current_file[0] = '\0';
}

static int editor_load_file(const char *path)
{
    if (!path || !editor || strlen(path) >= MAX_FILE_PATH)
        return -1;

    FILE *file = fopen(path, "rb");

    if (!file)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    long size = ftell(file);

    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    char *buffer = malloc((size_t)size + 1);

    if (!buffer) {
        fclose(file);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, file);

    fclose(file);

    if (bytes_read != (size_t)size) {
        free(buffer);
        return -1;
    }

    buffer[bytes_read] = '\0';

    editbox_set_text(editor, buffer);

    free(buffer);

    editor_set_current_file(path);
    document_open = 1;

    return 0;
}

static int editor_save_file(const char *path)
{
    if (!path || !editor || strlen(path) >= MAX_FILE_PATH)
        return -1;

    editbox_data_t *data = (editbox_data_t *)editor->data;

    if (!data || !data->text)
        return -1;

    FILE *file = fopen(path, "wb");

    if (!file)
        return -1;

    size_t length = strlen(data->text);
    size_t written = fwrite(data->text, 1, length, file);

    int result = fclose(file);

    if (written != length || result != 0)
        return -1;

    editor_set_current_file(path);
    document_open = 1;

    return 0;
}

/*
 * NOVO DOCUMENTO
 */
void ed_click_new(component_t *c)
{
    (void)c;

    if (!editor)
        return;

    editor_clear_document();

    document_open = 1;

    printf("Novo documento criado.\n");
}

/*
 * ABRIR DOCUMENTO
 */
void ed_click_open(component_t *c)
{
    (void)c;

    if (!win || !editor)
        return;

    char *selected_path = NULL;

    if (ui_open_file(
        win,
        "/",
        "",
        &selected_path
    ) == 0) {

        if (editor_load_file(selected_path) == 0) {
            printf("Ficheiro aberto: %s\n", selected_path);
        } else {
            printf("Erro ao abrir: %s\n", selected_path);
        }

        free(selected_path);
    }
}

/*
 * SALVAR DOCUMENTO
 */
void ed_click_save(component_t *c)
{
    (void)c;

    if (!win || !editor || !document_open)
        return;

    /*
     * Se já existe um caminho associado,
     * salva directamente no mesmo ficheiro.
     */
    if (current_file[0] != '\0') {

        if (editor_save_file(current_file) == 0) {
            printf("Ficheiro salvo: %s\n", current_file);
        } else {
            printf("Erro ao salvar: %s\n", current_file);
        }

        return;
    }

    /*
     * Documento novo: apresenta o diálogo
     * para escolher o nome e localização.
     */
    char *selected_path = NULL;

    if (ui_save_file(
        win,
        "/mnt/hd0",
        "novo.txt",
        &selected_path
    ) == 0) {

        if (editor_save_file(selected_path) == 0) {
            printf("Ficheiro salvo: %s\n", selected_path);
        } else {
            printf("Erro ao salvar: %s\n", selected_path);
        }

        free(selected_path);
    }
}

/*
 * FECHAR DOCUMENTO
 */
void ed_click_close(component_t *c)
{
    (void)c;

    if (!editor || !document_open)
        return;

    editor_clear_document();

    //document_open = 0;

    printf("Documento fechado.\n");
}

/*
 * VISUALIZAR CONTEÚDO
 */
void ed_click_view(component_t *c)
{
    (void)c;

    if (!editor)
        return;

    editbox_data_t *data = (editbox_data_t *)editor->data;

    if (data && data->text)
        printf("%s\n", data->text);
}

int main(void)
{
    document_open = 1;

    int socket_fd = procyon_init_client();

    if (socket_fd < 0)
        return -1;

    win = window_create(
        30,
        20,
        600,
        600,
        "Editor",
        argb(255, 0xCC, 0xCC, 0xCC),
        0xFF000000
    );

    if (!win) {
        procyon_finish_client(socket_fd);
        return -2;
    }

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

    window_controls_t controls;

    window_controls_init(
        &ui,
        win,
        &controls,
        WINDOW_CONTROLS_ALL
    );

    /*
     * MENU
     */
    menu.ui = &ui;
    menu.start_x = win->client.x;
    menu.start_y = win->client.y + 4;
    menu.spacing_x = 12;
    menu.count = 0;

    menu_add_button("Novo", ed_click_new);
    menu_add_button("Abrir", ed_click_open);
    menu_add_button("Salvar", ed_click_save);
    menu_add_button("Fechar", ed_click_close);
    menu_add_button("Ver", ed_click_view);

    /*
     * EDITBOX
     */
    editor = create_simple_editbox(
        &ui,
        win->client.x,
        win->client.y + MENU_BUTTON_HEIGHT + 10,
        win->client.width,
        win->client.height - 30,
        18,
        EDITBOX_TEXT
    );

    if (!editor) {
        ui_destroy_all_components(&ui);
        enviar_remocao_janela(socket_fd, win);
        window_destroy(win);
        procyon_finish_client(socket_fd);
        return -3;
    }

    editbox_data_t *e = (editbox_data_t *)editor->data;

    e->border_radius = 0;
    e->border_thickness = 0;
    e->ctx->window->bg = argb(255, 245, 245, 245);
    e->scrollbar_bg = e->ctx->window->bg;

    editor->fg = argb(255, 0, 0, 0);
    editor->border_color = e->ctx->window->bg;
    editor->bg = e->ctx->window->bg;

    editbox_set_multiline(editor, 1);

    editbox_set_scrollbars(
        editor,
        1,
        1
    );

    enviar_janela_ao_servidor(socket_fd, win, 1);

    /*
     * EVENT LOOP
     */
    while (1) {
        ui_draw(&ui);

        event_t ev = get_next_event_window(win);

        ui_event(&ui, ev);

        if (ev.type == EVENT_WINDOW_CLOSE)
            break;
    }

    /*
     * LIBERTAR COMPONENTES
     */
    ui_destroy_all_components(&ui);

    /*
     * REMOVER JANELA
     */
    if (enviar_remocao_janela(socket_fd, win) == 0)
        window_destroy(win);

    procyon_finish_client(socket_fd);

    return 0;
}