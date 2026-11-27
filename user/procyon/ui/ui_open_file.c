#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "include/ui_file_manager.h"

int ui_open_file(
    window_t *win,
    const char *initial_path,
    const char *initial_filename,
    char **selected_path
)
{
    if (!initial_path || !initial_filename || !selected_path)
        return -1;

    *selected_path = NULL;

    if (strlen(initial_path) >= UI_SAVE_PATH_MAX ||
        strlen(initial_filename) >= UI_SAVE_FILENAME_MAX)
        return -1;

    ui_save_shared_t *shared = mmap(
        NULL,
        sizeof(ui_save_shared_t),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (shared == MAP_FAILED)
        return -1;

    memset(shared, 0, sizeof(*shared));

    snprintf(
        shared->initial_path,
        sizeof(shared->initial_path),
        "%s",
        initial_path
    );

    snprintf(
        shared->initial_filename,
        sizeof(shared->initial_filename),
        "%s",
        initial_filename
    );

    pid_t pid = fork();

    if (pid < 0) {
        munmap(shared, sizeof(*shared));
        return -1;
    }

    if (pid == 0) {
        char window_id[32];
        char shared_ptr[32];

        snprintf(
            window_id,
            sizeof(window_id),
            "%lu",
            (unsigned long)win
        );

        snprintf(
            shared_ptr,
            sizeof(shared_ptr),
            "%llx",
            (unsigned long long)(uintptr_t)shared
        );

        char *argv[] = {
            "usr/app/file_manager_open",
            window_id,
            shared_ptr,
            NULL
        };

        execv(argv[0], argv);

        _exit(127);
    }

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            munmap(shared, sizeof(*shared));
            return -1;
        }
    }

    int result = -1;

    if (WIFEXITED(status) &&
        WEXITSTATUS(status) == 0 &&
        shared->result == 1) {

        size_t length = strnlen(
            shared->selected_path,
            sizeof(shared->selected_path)
        );

        if (length > 0 && length < sizeof(shared->selected_path)) {
            *selected_path = malloc(length + 1);

            if (*selected_path) {
                memcpy(
                    *selected_path,
                    shared->selected_path,
                    length + 1
                );

                result = 0;
            }
        }
    }

    munmap(shared, sizeof(*shared));

    return result;
}