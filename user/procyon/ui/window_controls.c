#include "include/components.h"
#include "include/iconbutton.h"
#include "include/window_controls.h"
#include <string.h>
#include <stdlib.h>

#define WINDOW_CONTROL_SIZE       24
#define WINDOW_CONTROL_SPACING     4
#define WINDOW_CONTROL_MARGIN      8

#define ICON_MINIMIZE "usr/share/icons/window-minimize.png"
#define ICON_MAXIMIZE "usr/share/icons/window-maximize.png"
#define ICON_CLOSE    "usr/share/icons/window-close.png"

#define MAX_WINDOW_CONTROL_BINDINGS 128

static window_control_binding_t g_bindings[MAX_WINDOW_CONTROL_BINDINGS];

static window_control_binding_t *window_control_find(component_t *comp)
{
    if (!comp)
        return NULL;

    for (int i = 0; i < MAX_WINDOW_CONTROL_BINDINGS; i++) {
        if (g_bindings[i].component == comp)
            return &g_bindings[i];
    }

    return NULL;
}

static window_control_binding_t *window_control_find_free(void)
{
    for (int i = 0; i < MAX_WINDOW_CONTROL_BINDINGS; i++) {
        if (!g_bindings[i].component)
            return &g_bindings[i];
    }

    return NULL;
}

static int window_control_bind(
    component_t *comp,
    window_t *window,
    window_control_binding_t *slot)
{
    if (!comp || !window || !slot)
        return -1;

    slot->component = comp;
    slot->window = window;
    slot->maximized = 0;
    return 0;
}

static void window_control_unbind_window(window_t *window)
{
    if (!window)
        return;

    for (int i = 0; i < MAX_WINDOW_CONTROL_BINDINGS; i++) {
        if (g_bindings[i].window == window)
            memset(&g_bindings[i], 0, sizeof(g_bindings[i]));
    }
}

static void window_control_invalidate(component_t *comp)
{
    if (!comp)
        return;

    comp->render = 1;
    comp->dirty = 1;
}

static component_t *window_control_create(
    ui_context_t *ui,
    window_t *window,
    const char *icon_path,
    void (*callback)(component_t *))
{
    if (!ui || !ui->window || !window || !icon_path || !callback)
        return NULL;

    window_control_binding_t *slot = window_control_find_free();
    if (!slot)
        return NULL;

    component_t *button = create_iconbutton(
        ui,
        0,
        0,
        WINDOW_CONTROL_SIZE,
        WINDOW_CONTROL_SIZE,
        "",
        icon_path,
        18,
        callback
    );

    if (!button)
        return NULL;

    button->bg = ui->window->bg;
    button->fg = 0xFFFFFFFF;
    button->border_color = ui->window->bg;

    iconbutton_data_t *data = (iconbutton_data_t *)button->data;
    if (data) {
        data->show_label = 0;
        data->layout = ICON_CENTER;
        data->bg_normal = ui->window->bg;
        data->border_thickness = 0;
        data->border_radius = 0;
    }

    if (window_control_bind(button, window, slot) != 0)
        return NULL;

    window_control_invalidate(button);
    return button;
}

static void window_control_minimize_callback(component_t *comp)
{
    window_control_binding_t *binding = window_control_find(comp);

    if (!binding || !binding->window)
        return;

    window_minimize(binding->window);
}

static void window_control_maximize_callback(component_t *comp)
{
    window_control_binding_t *binding = window_control_find(comp);

    if (!binding || !binding->window)
        return;

    if (binding->maximized) {
        window_restore(binding->window);
        binding->maximized = 0;
    } else {
        window_maximize(binding->window);
        binding->maximized = 1;
    }
}

static void window_control_close_callback(component_t *comp)
{
    if (!comp || !comp->ctx)
        return;

    window_control_binding_t *binding = window_control_find(comp);
    if (!binding || !binding->window)
        return;

    window_t *window = binding->window;

    if (enviar_remocao_janela(comp->ctx->fd, window) != 0)
        return;


    window_control_unbind_window(window);
    window_close(window);

    exit(0);
}

static void window_controls_recalculate_layout(
    window_t *window,
    window_controls_t *controls)
{
    if (!window || !controls)
        return;

    int x = window->width - WINDOW_CONTROL_MARGIN - WINDOW_CONTROL_SIZE;
    int y = (WINDOW_TITLEBAR_HEIGHT - WINDOW_CONTROL_SIZE) / 2;

    if (controls->close) {
        controls->close->x = x;
        controls->close->y = y;

        iconbutton_data_t *data =
            (iconbutton_data_t *)controls->close->data;
        if (data)
            data->bg_hover = argb(255, 200, 0, 0);

        window_control_invalidate(controls->close);
        x -= WINDOW_CONTROL_SIZE + controls->spacing;
    }

    if (controls->maximize) {
        controls->maximize->x = x;
        controls->maximize->y = y;
        window_control_invalidate(controls->maximize);
        x -= WINDOW_CONTROL_SIZE + controls->spacing;
    }

    if (controls->minimize) {
        controls->minimize->x = x;
        controls->minimize->y = y;
        window_control_invalidate(controls->minimize);
    }
}

void window_controls_init(
    ui_context_t *ui,
    window_t *window,
    window_controls_t *controls,
    uint32_t flags)
{
    if (!ui || !ui->window || !window || !controls)
        return;

    memset(controls, 0, sizeof(*controls));

    controls->flags = flags;
    controls->spacing = WINDOW_CONTROL_SPACING;

    if (flags & WINDOW_CONTROL_MINIMIZE) {
        controls->minimize = window_control_create(
            ui, window, ICON_MINIMIZE,
            window_control_minimize_callback
        );
    }

    if (flags & WINDOW_CONTROL_MAXIMIZE) {
        controls->maximize = window_control_create(
            ui, window, ICON_MAXIMIZE,
            window_control_maximize_callback
        );
    }

    if (flags & WINDOW_CONTROL_CLOSE) {
        controls->close = window_control_create(
            ui, window, ICON_CLOSE,
            window_control_close_callback
        );
    }

    window_controls_recalculate_layout(window, controls);
}

/*
 * Estas operações dependem do modelo de janelas do sistema:
 * estados de minimização/maximização, geometria original e integração
 * com o compositor/gestor de janelas. Não é seguro presumir campos
 * de window_t que não foram apresentados.
 */
void window_minimize(window_t *window)
{
    if (!window)
        return;
}

void window_maximize(window_t *window)
{
    if (!window)
        return;
}

void window_restore(window_t *window)
{
    if (!window)
        return;
}

void window_close(window_t *window)
{
    if (!window)
        return;

    window_destroy(window);
}
