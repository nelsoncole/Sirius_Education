#include "include/components.h"
#include <stdlib.h>

int ui_add_component(ui_context_t *ctx, component_t *component)
{
    if (!ctx || !component)
        return -1;

    component_t **new_components =
        realloc(ctx->components,
                (ctx->count + 1) * sizeof(component_t *));

    if (!new_components)
        return -1;

    ctx->components = new_components;

    component->ctx = ctx;

    ctx->components[ctx->count++] = component;

    if (ctx->window)
        ctx->window->invalid = 1;

    return 0;
}

void ui_draw(ui_context_t* ctx)
{
    if (!ctx || !ctx->window)
        return;

    int render = 0;

    ui_lock(&ctx->lock);
    
    for (int i = 0; i < ctx->count; i++) {
        component_t* c = ctx->components[i];

        if (!c || !c->on_draw || !c->visible)
            continue;

        render += c->on_draw(c, ctx->window);
    }

    if (render > 0)
        avisar_redesenho_ao_servidor(ctx->fd, ctx->window);

    ui_unlock(&ctx->lock);
}

void ui_event(ui_context_t* ctx, event_t ev)
{
    if (!ctx || ev.type == EVENT_NONE)
        return;

    if (ctx->modal_active && ctx->modal_owner) {
        if (ctx->modal_owner->on_event)
            ctx->modal_owner->on_event(ctx->modal_owner, ev);
        return;
    }

    for (int i = ctx->count - 1; i >= 0; i--) {
        component_t* c = ctx->components[i];

        if (!c || !c->on_event)
            continue;

        c->on_event(c, ev);
    }
}
