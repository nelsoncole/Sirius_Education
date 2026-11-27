#ifndef WINDOW_CONTROLS_H
#define WINDOW_CONTROLS_H

#include "components.h"

#define WINDOW_CONTROL_MINIMIZE  (1 << 0)
#define WINDOW_CONTROL_MAXIMIZE  (1 << 1)
#define WINDOW_CONTROL_CLOSE     (1 << 2)

#define WINDOW_CONTROLS_ALL \
    (WINDOW_CONTROL_MINIMIZE | WINDOW_CONTROL_MAXIMIZE | WINDOW_CONTROL_CLOSE)

typedef struct {
    component_t *minimize;
    component_t *maximize;
    component_t *close;

    uint32_t flags;
    int spacing;

    window_t *window;
    int maximized;
} window_controls_t;

typedef struct {
    component_t *component;
    window_t *window;
    int maximized;
} window_control_binding_t;

void window_controls_init(
    ui_context_t *ui,
    window_t *window,
    window_controls_t *controls,
    uint32_t flags
);


void window_minimize(window_t *window);
void window_maximize(window_t *window);
void window_restore(window_t *window);
void window_close(window_t *window);

#endif