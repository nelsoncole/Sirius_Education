#include "include/components.h"
#include "../procyon_ipc.h"

void ui_draw(ui_context_t* ctx) {
    if (!ctx || !ctx->window) return;
    int render = 0;
    for (int i = 0; i < ctx->count; i++) {
        component_t* c = ctx->components[i];

        if (!c || !c->on_draw) continue;
        if (!c->visible) continue;

        render += c->on_draw(c, ctx->window);
    }

    if(render > 0) {
        avisar_redesenho_ao_servidor(ctx->fd, ctx->window);
    }
}

void ui_event(ui_context_t* ctx, event_t ev) {
    if (!ctx || ev.type == EVENT_NONE) return;

    if (ctx->modal_active && ctx->modal_owner) {
        ctx->modal_owner->on_event(ctx->modal_owner, ev);
        return;
    }

    for (int i = 0; i < ctx->count; i++) {
        component_t* c = ctx->components[i];
        if (!c || !c->on_event) continue;
        c->on_event(c, ev);
    }
}