#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stddef.h>
#include <procyon/procyon.h>

typedef enum {
    FILE_MANAGER_MODE = 0,
    FILE_MANAGER_OPEN = 1,
    FILE_MANAGER_SAVE = 2
} file_manager_mode_t;

typedef struct file_manager file_manager_t;

typedef void (*file_manager_result_callback_t)(const char *path, void *userdata);

file_manager_t *file_manager_create(
    ui_context_t *ui,
    int x, int y, int width, int height,
    const char *initial_path,
    file_manager_mode_t mode,
    char *file_buffer,
    size_t file_buffer_size
);

void file_manager_set_result_callback(
    file_manager_t *fm,
    file_manager_result_callback_t callback,
    void *userdata
);

const char *file_manager_get_selected_path(file_manager_t *fm);
void file_manager_destroy(file_manager_t *fm);
int file_manager_refresh(file_manager_t *fm);
int file_manager_go_to(file_manager_t *fm, const char *path);
int file_manager_go_up(file_manager_t *fm);

#endif
