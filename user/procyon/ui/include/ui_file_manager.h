#ifndef UI_FILE_MANAGER_H
#define UI_FILE_MANAGER_H


#include <limits.h>

#include "components.h"

#define UI_SAVE_PATH_MAX PATH_MAX
#define UI_SAVE_FILENAME_MAX 256

typedef struct {
    int result;
    char initial_path[UI_SAVE_PATH_MAX];
    char initial_filename[UI_SAVE_FILENAME_MAX];
    char selected_path[UI_SAVE_PATH_MAX];
} ui_save_shared_t;

int ui_open_file(
    window_t *win,
    const char *initial_path,
    const char *initial_filename,
    char **selected_path
);

int ui_save_file(
    window_t *win,
    const char *initial_path,
    const char *initial_filename,
    char **selected_path
);

#endif
