
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "desktop.h"

static char *trim(char *str)
{
    while (isspace((unsigned char)*str))
        str++;

    char *end = str + strlen(str);

    while (end > str && isspace((unsigned char)end[-1]))
        *--end = '\0';

    return str;
}

static void start_app_entry_free(start_app_entry_t *app)
{
    if (!app)
        return;

    free(app->name);
    free(app->icon);
    free(app->command);

    memset(app, 0, sizeof(*app));
}

static int start_app_list_add(
    start_app_list_t *list,
    start_app_entry_t *app)
{
    if (!list || !app)
        return -1;

    for (unsigned int i = 0; i < list->count; i++)
    {
        if (list->apps[i].id == app->id)
            return -1;
    }

    if (list->count >= list->capacity)
    {
        unsigned int capacity = list->capacity ?
                                list->capacity * 2 : 8;

        if (capacity <= list->capacity)
            return -1;

        start_app_entry_t *apps = realloc(
            list->apps,
            capacity * sizeof(start_app_entry_t)
        );

        if (!apps)
            return -1;

        list->apps = apps;
        list->capacity = capacity;
    }

    list->apps[list->count++] = *app;

    memset(app, 0, sizeof(*app));

    return 0;
}

int start_menu_load_config(
    const char *path,
    start_app_list_t *list)
{
    printf("[START_MENU] Iniciando carregamento da configuracao\n");
    printf("[START_MENU] Path: %s\n", path ? path : "(null)");

    if (!path || !list)
    {
        printf("[START_MENU][ERROR] Path ou list invalido\n");
        return -1;
    }

    FILE *file = fopen(path, "r");

    if (!file)
    {
        printf("[START_MENU][ERROR] Nao foi possivel abrir o ficheiro\n");
        return -1;
    }

    printf("[START_MENU] Ficheiro aberto com sucesso\n");

    start_app_entry_t app = {0};
    char line[512];
    int in_app = 0;
    int result = 0;
    unsigned int line_number = 0;

    while (line_number < 100)
    {
        memset(line, 0, sizeof(line));

        char *ret = fgets(line, sizeof(line), file);

        if (!ret)
        {
            break;
        }

        line_number++;

        char *value = trim(line);

        if (!*value || *value == '#' || *value == ';')
        {
            continue;
        }

        if (strcmp(value, "[app]") == 0)
        {

            if (in_app)
            {

                if (!app.id || !app.name ||
                    !app.icon || !app.command ||
                    start_app_list_add(list, &app) < 0)
                {
                    start_app_entry_free(&app);
                    result = -1;
                    break;
                }

            }

            memset(&app, 0, sizeof(app));
            in_app = 1;
            continue;
        }

        if (!in_app)
        {
            continue;
        }

        char *separator = strchr(value, '=');

        if (!separator)
        {
            continue;
        }

        *separator = '\0';

        char *key = trim(value);
        char *data = trim(separator + 1);

        if (!*data)
        {
            continue;
        }

        if (strcmp(key, "id") == 0)
        {
            char *end;
            unsigned long id = strtoul(data, &end, 10);

            if (*end || id == 0 || id > UINT_MAX)
            {
                result = -1;
                break;
            }

            app.id = (unsigned int)id;
        }
        else if (strcmp(key, "name") == 0)
        {
            free(app.name);
            app.name = strdup(data);
        }
        else if (strcmp(key, "icon") == 0)
        {
            free(app.icon);
            app.icon = strdup(data);
        }
        else if (strcmp(key, "command") == 0)
        {
            free(app.command);
            app.command = strdup(data);
        }
        else
        {
            printf("[START_MENU][WARNING] Campo desconhecido: %s\n", key);
        }

        if (line_number >= 100)
        {
            printf("[START_MENU][ERROR] Limite de 100 linhas atingido. Possivel loop infinito.\n");
            result = -1;
            break;
        }
    }


    if (result == 0 && in_app)
    {

        if (!app.id || !app.name ||
            !app.icon || !app.command ||
            start_app_list_add(list, &app) < 0)
        {
            result = -1;
        }
    }

    start_app_entry_free(&app);
    fclose(file);

    printf("[START_MENU] Resultado: %d | Aplicacoes carregadas: %u\n",
           result, list->count);

    return result;
}

void start_app_list_free(start_app_list_t *list)
{
    if (!list)
        return;

    for (unsigned int i = 0; i < list->count; i++)
        start_app_entry_free(&list->apps[i]);

    free(list->apps);
    memset(list, 0, sizeof(*list));
}