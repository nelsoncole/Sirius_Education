#ifndef DESKTOP_H
#define DESKTOP_H

typedef struct
{
    unsigned int id;
    char *name;
    char *icon;
    char *command;
} start_app_entry_t;

typedef struct
{
    start_app_entry_t *apps;
    unsigned int count;
    unsigned int capacity;
} start_app_list_t;

int start_menu_load_config(const char *path, start_app_list_t *list);
void start_app_list_free(start_app_list_t *list);

#endif