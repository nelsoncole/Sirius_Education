#define _XOPEN_SOURCE 700

#include "file_manager.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

volatile int g_final;

#define FM_TOOLBAR_HEIGHT 50
#define FM_FILENAME_HEIGHT 32
#define FM_ACTION_HEIGHT 32
#define FM_BOTTOM_SPACING 8
#define FM_ACTION_BUTTON_WIDTH 100
#define FM_EDITBOX_FONT_SIZE 18
#define FM_SIDEBAR_WIDTH 190
#define FM_TOOLBAR_ITEM_WIDTH 40
#define FM_TOOLBAR_ITEM_HEIGHT 40
#define FM_TOOLBAR_ITEM_SPACING_X 10
#define FM_TOOLBAR_ICON_SIZE 24
#define FM_TOOLBAR_ITEM_CALCULE_X(n) (n*FM_TOOLBAR_ITEM_WIDTH + n*FM_TOOLBAR_ITEM_SPACING_X + FM_TOOLBAR_ITEM_SPACING_X)

#define FM_FONT_SIZE 18
#define FM_SIDEBAR_ICON_SIZE 24
#define FM_FILE_ICON_SIZE 48

#define FM_SIDEBAR_ROW_HEIGHT 42

#define FM_ITEM_WIDTH 104
#define FM_ITEM_HEIGHT 104
#define FM_ITEM_SPACING_X 10
#define FM_ITEM_SPACING_Y 10

#define FM_COLOR_BACKGROUND 0xFFF4F5F7
#define FM_COLOR_FOREGROUND 0xFF252A32

#define FM_HISTORY_MAX 64

struct file_manager {
    ui_context_t *ui;
    component_t *sidebar;
    component_t *files;
    component_t *filename;
    component_t *action_button;

    file_manager_mode_t mode;
    char selected_path[PATH_MAX];
    char *file_buffer;
    size_t file_buffer_size;
    file_manager_result_callback_t result_callback;
    void *result_userdata;

    char current_path[PATH_MAX];
    char home_path[PATH_MAX];

    char history[FM_HISTORY_MAX][PATH_MAX];
    int history_count;
    int history_index;
};

static file_manager_t *g_file_manager;

static void fm_invalidate(file_manager_t *fm);

static void fm_copy_string(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0)
        return;

    snprintf(dst, size, "%s", src ? src : "");
}

static int fm_join_path(
    char *dst,
    size_t size,
    const char *base,
    const char *name
)
{
    int length;

    if (!dst || size == 0 || !base || !name)
        return -1;

    if (strcmp(base, "/") == 0)
        length = snprintf(dst, size, "/%s", name);
    else
        length = snprintf(dst, size, "%s/%s", base, name);

    return length < 0 || (size_t)length >= size ? -1 : 0;
}

static void fm_set_button_style(
    component_t *button,
    int layout,
    uint32_t background
)
{
    if (!button || !button->data)
        return;

    iconbutton_data_t *data = button->data;

    button->bg = background;
    button->fg = FM_COLOR_FOREGROUND;
    button->border_color = background;
    button->font_size = FM_FONT_SIZE;

    data->bg_normal = background;
    data->border_thickness = 0;
    data->border_radius = 4;
    data->layout = layout;
}

static component_t *fm_create_button(
    ui_context_t *ui,
    int x,
    int y,
    int width,
    int height,
    const char *label,
    const char *icon,
    int icon_size,
    void (*callback)(component_t *)
)
{
    component_t *button = create_iconbutton(
        ui,
        x,
        y,
        width,
        height,
        label,
        icon,
        icon_size,
        callback
    );

    if (button) {
        fm_set_button_style(
            button,
            ICON_LABEL_BOTTOM,
            FM_COLOR_BACKGROUND
        );
    }

    if (button && button->data) {
        iconbutton_data_t *b = (iconbutton_data_t *)button->data;
        b->show_label = 0;
    }

    return button;
}

static void fm_invalidate(file_manager_t *fm)
{
    if (!fm)
        return;

    if (fm->sidebar)
        fm->sidebar->render = 1;

    if (fm->files)
        fm->files->render = 1;

    if (fm->ui && fm->ui->window)
        fm->ui->window->invalid = 1;
}

static void fm_clear_scroll(component_t *scroll)
{
    if (scroll)
        ui_clear_component(scroll);
}

static void fm_set_filename(file_manager_t *fm, const char *name)
{
    if (!fm || !fm->filename || !name)
        return;

    editbox_set_text(fm->filename, name);
}

static void fm_item_click(component_t *button)
{
    if (!button || !button->command[0] || !g_file_manager)
        return;

    file_manager_t *fm = g_file_manager;
    struct stat st;

    if (stat(button->command, &st) != 0)
        return;

    if (S_ISDIR(st.st_mode)) {
        file_manager_go_to(fm, button->command);
        return;
    }

    if (fm->mode == FILE_MANAGER_OPEN || fm->mode == FILE_MANAGER_SAVE) {
        const char *name = strrchr(button->command, '/');
        fm_set_filename(fm, name ? name + 1 : button->command);
    }
}

static void fm_action_click(component_t *button)
{
    (void)button;

    file_manager_t *fm = g_file_manager;
    if (!fm || fm->mode == FILE_MANAGER_MODE || !fm->filename)
        return;

    editbox_data_t *edit = (editbox_data_t *)fm->filename->data;
    if (!edit || !edit->text)
        return;

    if (strlen(edit->text) >= PATH_MAX)
        return;

    char name[PATH_MAX];
    snprintf(name, sizeof(name), "%s", edit->text);

    size_t length = strlen(name);
    while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == '\t'))
        name[--length] = '\0';

    char *start = name;
    while (*start == ' ' || *start == '\t')
        start++;

    if (!*start || strcmp(start, ".") == 0 || strcmp(start, "..") == 0)
        return;

    if (strchr(start, '/') || strchr(start, '\\'))
        return;

    char full_path[PATH_MAX];
    if (fm_join_path(full_path, sizeof(full_path), fm->current_path, start) != 0)
        return;

    struct stat st;

    if (fm->mode == FILE_MANAGER_OPEN) {
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode))
            return;
    } else if (fm->mode == FILE_MANAGER_SAVE) {
        /*if (access(fm->current_path, W_OK) != 0)
            return;*/

        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode))
            return;
    } else {
        return;
    }

    if (!fm->file_buffer || fm->file_buffer_size == 0 ||
        strlen(full_path) >= fm->file_buffer_size ||
        strlen(full_path) >= sizeof(fm->selected_path))
        return;

    /*
     * OPEN devolve o caminho do ficheiro existente.
     * SAVE devolve o caminho de destino, sem criar ou gravar o ficheiro.
     */
    snprintf(fm->file_buffer, fm->file_buffer_size, "%s", full_path);
    snprintf(fm->selected_path, sizeof(fm->selected_path), "%s", full_path);

    fm_invalidate(fm);

    if (fm->result_callback)
        fm->result_callback(fm->selected_path, fm->result_userdata);

    g_final = 1;
}

static void fm_sidebar_click(component_t *button)
{
    if (!button || !button->command[0] || !g_file_manager)
        return;

    file_manager_go_to(g_file_manager, button->command);
}

static void fm_refresh_click(component_t *button)
{
    (void)button;

    if (g_file_manager)
        file_manager_refresh(g_file_manager);
}

static void fm_up_click(component_t *button)
{
    (void)button;

    if (g_file_manager)
        file_manager_go_up(g_file_manager);
}

static void fm_back_click(component_t *button)
{
    (void)button;

    file_manager_t *fm = g_file_manager;
    if (!fm || fm->history_index <= 0)
        return;

    fm->history_index--;
    snprintf(fm->current_path, sizeof(fm->current_path), "%s",
             fm->history[fm->history_index]);
    file_manager_refresh(fm);
}

static void fm_right_click(component_t *button)
{
    (void)button;

    file_manager_t *fm = g_file_manager;
    if (!fm || fm->history_index >= fm->history_count - 1)
        return;

    fm->history_index++;
    snprintf(fm->current_path, sizeof(fm->current_path), "%s",
             fm->history[fm->history_index]);
    file_manager_refresh(fm);
}

static void fm_home_click(component_t *button)
{
    (void)button;

    if (g_file_manager)
        file_manager_go_to(
            g_file_manager,
            g_file_manager->home_path
        );
}

static component_t *fm_add_scroll_button(
    component_t *scroll,
    const char *label,
    const char *icon,
    const char *path,
    int width,
    int height,
    int icon_size,
    int layout,
    void (*callback)(component_t *)
)
{
    if (!scroll || !scroll->data)
        return NULL;

    scroll_data_t *data = scroll->data;

    if (!data->ctx)
        return NULL;

    component_t *button = create_iconbutton(
        data->ctx,
        0,
        0,
        width,
        height,
        label,
        icon,
        icon_size,
        callback
    );

    if (!button)
        return NULL;

    iconbutton_data_t* b = (iconbutton_data_t*)button->data;
    b->spacing = 8;

    fm_set_button_style(
        button,
        layout,
        FM_COLOR_BACKGROUND
    );

    if (path) {
        fm_copy_string(
            button->command,
            sizeof(button->command),
            path
        );
    }

    if (scroll_add_component(scroll, button) != 0) {
        ui_destroy_component(button);
        return NULL;
    }

    return button;
}

static void fm_add_sidebar_entry(
    file_manager_t *fm,
    const char *label,
    const char *path,
    const char *icon
)
{
    if (!fm || !fm->sidebar || !path)
        return;

    struct stat st;

    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
        return;

    fm_add_scroll_button(
        fm->sidebar,
        label,
        icon,
        path,
        FM_SIDEBAR_WIDTH - 12,
        FM_SIDEBAR_ROW_HEIGHT,
        FM_SIDEBAR_ICON_SIZE,
        ICON_LABEL_RIGHT,
        fm_sidebar_click
    );
}

static const char *fm_get_file_icon(const char *name, int is_directory)
{
    if (is_directory)
        return "usr/share/icons/folder.png";

    if (!name)
        return "usr/share/icons/file.png";

    const char *extension = strrchr(name, '.');

    if (!extension || extension == name)
        return "usr/share/icons/file.png";

    if (strcasecmp(extension, ".txt") == 0 ||
        strcasecmp(extension, ".md") == 0 ||
        strcasecmp(extension, ".log") == 0 ||
        strcasecmp(extension, ".csv") == 0 ||
        strcasecmp(extension, ".ini") == 0 ||
        strcasecmp(extension, ".conf") == 0 ||
        strcasecmp(extension, ".json") == 0 ||
        strcasecmp(extension, ".xml") == 0 ||
        strcasecmp(extension, ".html") == 0 ||
        strcasecmp(extension, ".css") == 0 ||
        strcasecmp(extension, ".c") == 0 ||
        strcasecmp(extension, ".h") == 0 ||
        strcasecmp(extension, ".cpp") == 0 ||
        strcasecmp(extension, ".hpp") == 0 ||
        strcasecmp(extension, ".py") == 0 ||
        strcasecmp(extension, ".js") == 0 ||
        strcasecmp(extension, ".java") == 0)
        return "usr/share/icons/file-type-text.png";

    if (strcasecmp(extension, ".png") == 0 ||
        strcasecmp(extension, ".jpg") == 0 ||
        strcasecmp(extension, ".jpeg") == 0 ||
        strcasecmp(extension, ".gif") == 0 ||
        strcasecmp(extension, ".bmp") == 0 ||
        strcasecmp(extension, ".svg") == 0 ||
        strcasecmp(extension, ".webp") == 0 ||
        strcasecmp(extension, ".tiff") == 0)
        return "usr/share/icons/file-type-image.png";

    if (strcasecmp(extension, ".mp4") == 0 ||
        strcasecmp(extension, ".mkv") == 0 ||
        strcasecmp(extension, ".avi") == 0 ||
        strcasecmp(extension, ".mov") == 0 ||
        strcasecmp(extension, ".webm") == 0 ||
        strcasecmp(extension, ".mpeg") == 0 ||
        strcasecmp(extension, ".mpg") == 0)
        return "usr/share/icons/file-type-video.png";

    if (strcasecmp(extension, ".mp3") == 0 ||
        strcasecmp(extension, ".wav") == 0 ||
        strcasecmp(extension, ".ogg") == 0 ||
        strcasecmp(extension, ".flac") == 0 ||
        strcasecmp(extension, ".aac") == 0 ||
        strcasecmp(extension, ".m4a") == 0)
        return "usr/share/icons/file-type-music.png";

    if (strcasecmp(extension, ".pdf") == 0 ||
        strcasecmp(extension, ".doc") == 0 ||
        strcasecmp(extension, ".docx") == 0 ||
        strcasecmp(extension, ".odt") == 0 ||
        strcasecmp(extension, ".rtf") == 0 ||
        strcasecmp(extension, ".xls") == 0 ||
        strcasecmp(extension, ".xlsx") == 0 ||
        strcasecmp(extension, ".ppt") == 0 ||
        strcasecmp(extension, ".pptx") == 0)
        return "usr/share/icons/file-type-doc.png";

    return "usr/share/icons/file.png";
}

static void fm_build_sidebar(file_manager_t *fm)
{
    if (!fm || !fm->sidebar)
        return;

    fm_clear_scroll(fm->sidebar);

    fm_add_sidebar_entry(
        fm,
        "Pasta Pessoal",
        fm->home_path,
        "usr/share/icons/home.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Sistema",
        "/",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Documentos",
        "home/Documentos",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Imagens",
        "home/Imagens",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Transferências",
        "home/Transferencias",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Música",
        "home/Musica",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Vídeos",
        "home/Videos",
        "usr/share/icons/folder.png"
    );

    fm_add_sidebar_entry(
        fm,
        "Temporários",
        "/tmp",
        "usr/share/icons/folder.png"
    );

    scroll_recalculate_layout(fm->sidebar);
}

static component_t *fm_add_entry(
    file_manager_t *fm,
    const char *name,
    const char *path,
    const struct stat *st
)
{
    if (!fm || !fm->files || !name || !path || !st)
        return NULL;

    scroll_data_t *scroll_data = fm->files->data;

    if (!scroll_data || !scroll_data->ctx)
        return NULL;

    int is_directory = S_ISDIR(st->st_mode);
    const char *icon = fm_get_file_icon(name, is_directory);

    component_t *item = create_iconbutton(
        scroll_data->ctx,
        0,
        0,
        FM_ITEM_WIDTH,
        FM_ITEM_HEIGHT,
        name,
        icon,
        FM_FILE_ICON_SIZE,
        fm_item_click
    );

    if (!item)
        return NULL;

    fm_set_button_style(
        item,
        ICON_LABEL_BOTTOM,
        FM_COLOR_BACKGROUND
    );

    item->id = is_directory ? 1 : 2;

    fm_copy_string(
        item->command,
        sizeof(item->command),
        path
    );

    if (scroll_add_component(fm->files, item) != 0) {
        ui_destroy_component(item);
        return NULL;
    }

    return item;
}

int file_manager_refresh(file_manager_t *fm)
{
    if (!fm || !fm->files)
        return -1;

    fm->selected_path[0] = '\0';

    DIR *dir = opendir(fm->current_path);

    if (!dir)
        return -1;

    fm_clear_scroll(fm->files);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        struct stat st;

        if (fm_join_path(
                full_path,
                sizeof(full_path),
                fm->current_path,
                entry->d_name) != 0)
            continue;

        if (stat(full_path, &st) != 0)
            continue;

        fm_add_entry(
            fm,
            entry->d_name,
            full_path,
            &st
        );
    }

    closedir(dir);

    scroll_recalculate_layout(fm->files);
    fm_invalidate(fm);

    return 0;
}

static void fm_history_add(file_manager_t *fm, const char *path)
{
    if (!fm || !path)
        return;

    if (fm->history_index >= 0 &&
        fm->history_index < fm->history_count &&
        strcmp(fm->history[fm->history_index], path) == 0)
        return;

    fm->history_count = fm->history_index + 1;

    if (fm->history_count >= FM_HISTORY_MAX) {
        memmove(
            fm->history,
            fm->history + 1,
            sizeof(fm->history[0]) * (FM_HISTORY_MAX - 1)
        );
        fm->history_count = FM_HISTORY_MAX - 1;
        fm->history_index = FM_HISTORY_MAX - 2;
    }

    fm_copy_string(
        fm->history[fm->history_count],
        PATH_MAX,
        path
    );

    fm->history_index = fm->history_count;
    fm->history_count++;
}

int file_manager_go_to(file_manager_t *fm, const char *path)
{
    if (!fm || !path || !*path)
        return -1;

    char resolved[PATH_MAX];
    struct stat st;

    if (!realpath(path, resolved))
        return -1;

    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;

    fm_history_add(fm, resolved);

    fm_copy_string(
        fm->current_path,
        sizeof(fm->current_path),
        resolved
    );

    return file_manager_refresh(fm);
}

int file_manager_go_up(file_manager_t *fm)
{
    if (!fm)
        return -1;

    char parent[PATH_MAX];

    fm_copy_string(
        parent,
        sizeof(parent),
        fm->current_path
    );

    char *slash = strrchr(parent, '/');

    if (!slash)
        return file_manager_go_to(fm, "/");

    if (slash == parent)
        parent[1] = '\0';
    else
        *slash = '\0';

    return file_manager_go_to(fm, parent);
}

void file_manager_set_result_callback(
    file_manager_t *fm,
    file_manager_result_callback_t callback,
    void *userdata
)
{
    if (!fm)
        return;

    fm->result_callback = callback;
    fm->result_userdata = userdata;
}

const char *file_manager_get_selected_path(file_manager_t *fm)
{
    if (!fm || !fm->selected_path[0])
        return NULL;

    return fm->selected_path;
}

void file_manager_destroy(file_manager_t *fm)
{
    if (!fm)
        return;

    if (g_file_manager == fm)
        g_file_manager = NULL;

    /*
     * Os componentes visuais pertencem ao ui_context_t.
     * A aplicação deve chamar ui_destroy_all_components() antes
     * de destruir a janela/contexto da interface.
     */
    free(fm);
}

file_manager_t *file_manager_create(
    ui_context_t *ui,
    int x,
    int y,
    int width,
    int height,
    const char *initial_path,
    file_manager_mode_t mode,
    char *file_buffer,
    size_t file_buffer_size
)
{
    if (!ui || !ui->window ||
        (mode != FILE_MANAGER_MODE &&
         mode != FILE_MANAGER_OPEN &&
         mode != FILE_MANAGER_SAVE) ||
        (mode != FILE_MANAGER_MODE && (!file_buffer || file_buffer_size == 0)) ||
        width <= FM_SIDEBAR_WIDTH + 100 ||
        height <= FM_TOOLBAR_HEIGHT + 100)
        return NULL;

    file_manager_t *fm = calloc(1, sizeof(file_manager_t));

    if (!fm)
        return NULL;

    fm->ui = ui;
    fm->mode = mode;
    fm->file_buffer = file_buffer;
    fm->file_buffer_size = file_buffer_size;
    fm->history_index = -1;

    if (!initial_path || !*initial_path)
        initial_path = "/";

    if (!realpath(initial_path, fm->current_path)) {
        free(fm);
        return NULL;
    }

    fm_copy_string(
        fm->home_path,
        sizeof(fm->home_path),
        fm->current_path
    );

    int content_y = y + FM_TOOLBAR_HEIGHT;
    int bottom_controls_height = 0;

    if (mode != FILE_MANAGER_MODE)
        bottom_controls_height = 2 + FM_BOTTOM_SPACING + FM_ACTION_HEIGHT +
                                 FM_BOTTOM_SPACING;

    int content_height = height - FM_TOOLBAR_HEIGHT - bottom_controls_height;

    if (content_height < 60)
        goto fail;

    create_panel(ui, x, y + (FM_TOOLBAR_HEIGHT-2),
                 width, 2, 0xFF888888, 1, 0xFFAAAAAA);

    fm_create_button(
        ui,
        x + FM_TOOLBAR_ITEM_CALCULE_X(0),
        y + (FM_TOOLBAR_HEIGHT-FM_TOOLBAR_ITEM_HEIGHT)/2,
        FM_TOOLBAR_ITEM_WIDTH,
        FM_TOOLBAR_ITEM_HEIGHT,
        "Subir",
        "usr/share/icons/arrow-left.png",
        FM_TOOLBAR_ICON_SIZE,
        fm_back_click
    );
    
    fm_create_button(
        ui,
        x + FM_TOOLBAR_ITEM_CALCULE_X(1),
        y + (FM_TOOLBAR_HEIGHT-FM_TOOLBAR_ITEM_HEIGHT)/2,
        FM_TOOLBAR_ITEM_WIDTH,
        FM_TOOLBAR_ITEM_HEIGHT,
        "Subir",
        "usr/share/icons/arrow-right.png",
        FM_TOOLBAR_ICON_SIZE,
        fm_right_click
    );

    fm_create_button(
        ui,
        x + FM_TOOLBAR_ITEM_CALCULE_X(2),
        y + (FM_TOOLBAR_HEIGHT-FM_TOOLBAR_ITEM_HEIGHT)/2,
        FM_TOOLBAR_ITEM_WIDTH,
        FM_TOOLBAR_ITEM_HEIGHT,
        "Subir",
        "usr/share/icons/arrow-up.png",
        FM_TOOLBAR_ICON_SIZE,
        fm_up_click
    );

    fm_create_button(
        ui,
        x + FM_TOOLBAR_ITEM_CALCULE_X(3),
        y + (FM_TOOLBAR_HEIGHT-FM_TOOLBAR_ITEM_HEIGHT)/2,
        FM_TOOLBAR_ITEM_WIDTH,
        FM_TOOLBAR_ITEM_HEIGHT,
        "Início",
        "usr/share/icons/home.png",
        FM_TOOLBAR_ICON_SIZE,
        fm_home_click
    );

    fm_create_button(
        ui,
        x + FM_TOOLBAR_ITEM_CALCULE_X(4),
        y + (FM_TOOLBAR_HEIGHT-FM_TOOLBAR_ITEM_HEIGHT)/2,
        FM_TOOLBAR_ITEM_WIDTH,
        FM_TOOLBAR_ITEM_HEIGHT,
        "Actualizar",
        "usr/share/icons/refresh.png",
        FM_TOOLBAR_ICON_SIZE,
        fm_refresh_click
    );

    fm->sidebar = create_scroll(
        ui,
        x,
        content_y,
        FM_SIDEBAR_WIDTH,
        content_height
    );

    if (!fm->sidebar)
        goto fail;

    fm->files = create_scroll(
        ui,
        x + FM_SIDEBAR_WIDTH,
        content_y,
        width - FM_SIDEBAR_WIDTH,
        content_height
    );

    if (!fm->files)
        goto fail;

    scroll_set_layout(
        fm->sidebar,
        SCROLL_LAYOUT_VERTICAL,
        0,
        2,
        FM_SIDEBAR_WIDTH - 12,
        FM_SIDEBAR_ROW_HEIGHT
    );

    scroll_set_layout(
        fm->files,
        SCROLL_LAYOUT_GRID,
        FM_ITEM_SPACING_X,
        FM_ITEM_SPACING_Y,
        FM_ITEM_WIDTH,
        FM_ITEM_HEIGHT
    );

    if (mode != FILE_MANAGER_MODE) {
        int panel_y = y + height - bottom_controls_height;
        int row_y = panel_y + 2 + FM_BOTTOM_SPACING;
        int edit_x = x + FM_SIDEBAR_WIDTH + 10;
        int action_x = x + width - FM_ACTION_BUTTON_WIDTH - 10;
        int edit_width = action_x - edit_x - 10;

        if (edit_width < 80)
            goto fail;

        create_panel(
            ui,
            x,
            panel_y,
            width,
            2,
            0xFF888888,
            1,
            0xFFAAAAAA
        );

        fm->filename = create_editbox(
            ui,
            edit_x,
            row_y,
            edit_width,
            FM_FILENAME_HEIGHT,
            FM_EDITBOX_FONT_SIZE,
            EDITBOX_TEXT
        );

        if (!fm->filename)
            goto fail;

        editbox_data_t* e = (editbox_data_t*)fm->filename->data;
        e->border_radius = 8;
        e->border_thickness = 2;
        e->ctx->window->bg = ui->window->bg;
        fm->filename->bg = ui->window->bg;
        fm->filename->fg = argb(255, 0, 0 , 0);
        fm->filename->border_color = 0xFFAAAAAA;

        editbox_set_multiline(fm->filename, 0);
        editbox_set_vertical_alignment(fm->filename, EDITBOX_ALIGN_VCENTER);
        editbox_set_scrollbars(
            fm->filename,
            0, // Vertical
            0  // Horizontal
        );

        editbox_set_text(fm->filename,
            mode == FILE_MANAGER_SAVE ? file_buffer : "");

        fm->action_button = create_button(ui,
            action_x,
            row_y,
            FM_ACTION_BUTTON_WIDTH,
            FM_ACTION_HEIGHT,
            mode == FILE_MANAGER_OPEN ? "Abrir" : "Guardar",
            fm_action_click
        );
        
        if (!fm->action_button)
            goto fail;

        if (fm->action_button->data) {
            button_data_t *button_data =
                (button_data_t *)fm->action_button->data;
        }
    }

    g_file_manager = fm;

    fm_build_sidebar(fm);

    if (file_manager_refresh(fm) != 0)
        goto fail;

    fm_invalidate(fm);

    return fm;

fail:
    g_file_manager = NULL;

    if (fm->filename)
        ui_destroy_component(fm->filename);

    if (fm->files)
        ui_destroy_component(fm->files);

    if (fm->sidebar)
        ui_destroy_component(fm->sidebar);

    free(fm);
    return NULL;
}