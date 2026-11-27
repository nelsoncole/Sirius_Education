
#include "include/components.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int ui_rect_intersects(
    int ax, int ay, int aw, int ah,
    int bx, int by, int bw, int bh
)
{
    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0)
        return 0;

    long long ar = (long long)ax + aw;
    long long ab = (long long)ay + ah;
    long long br = (long long)bx + bw;
    long long bb = (long long)by + bh;

    return ax < br && ar > bx && ay < bb && ab > by;
}

static void ui_component_free(component_t *c)
{
    if (!c)
        return;

    if (c->on_destroy)
        c->on_destroy(c);

    c->ctx = NULL;
    free(c);
}

static void ui_component_release(component_t *c)
{
    if (!c)
        return;

    ui_component_free(c);
}

static void ui_notify_redraw(ui_context_t *ctx)
{
    if (!ctx || !ctx->window)
        return;

    if (ctx->suppress_server_notify)
        return;

    avisar_redesenho_ao_servidor(ctx->fd, ctx->window);
}

void ui_invalidate_region(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h
)
{
    if (!ctx || !ctx->window || w <= 0 || h <= 0)
        return;

    ui_lock(&ctx->lock);

    if (!ctx->full_redraw) {
        int merged = 0;

        for (int i = 0; i < ctx->damage_count; i++) {
            ui_damage_t *d = &ctx->damage[i];

            if (!ui_rect_intersects(
                    x, y, w, h,
                    d->x, d->y, d->w, d->h))
                continue;

            long long left = x < d->x ? x : d->x;
            long long top = y < d->y ? y : d->y;

            long long right_a = (long long)x + w;
            long long right_b = (long long)d->x + d->w;

            long long bottom_a = (long long)y + h;
            long long bottom_b = (long long)d->y + d->h;

            long long right = right_a > right_b ? right_a : right_b;
            long long bottom = bottom_a > bottom_b ? bottom_a : bottom_b;

            d->x = (int)left;
            d->y = (int)top;
            d->w = (int)(right - left);
            d->h = (int)(bottom - top);

            merged = 1;
            break;
        }

        if (!merged) {
            if (ctx->damage_count < UI_MAX_DAMAGE_REGIONS) {
                ctx->damage[ctx->damage_count++] = (ui_damage_t) {
                    .x = x,
                    .y = y,
                    .w = w,
                    .h = h
                };
            } else {
                ctx->damage_count = 0;
                ctx->full_redraw = 1;
            }
        }
    }

    ctx->window->invalid = 1;

    ui_unlock(&ctx->lock);

    ui_notify_redraw(ctx);
}

int ui_get_damage(
    ui_context_t *ctx,
    ui_damage_t *regions,
    int capacity,
    int *full_redraw
)
{
    if (!ctx || !full_redraw)
        return -1;

    ui_lock(&ctx->lock);

    *full_redraw = ctx->full_redraw;

    if (ctx->full_redraw) {
        ctx->full_redraw = 0;
        ctx->damage_count = 0;

        ui_unlock(&ctx->lock);
        return 0;
    }

    if (!regions || capacity <= 0) {
        ui_unlock(&ctx->lock);
        return 0;
    }

    int count = ctx->damage_count;

    if (count > capacity)
        count = capacity;

    for (int i = 0; i < count; i++)
        regions[i] = ctx->damage[i];

    for (int i = count; i < ctx->damage_count; i++)
        ctx->damage[i - count] = ctx->damage[i];

    ctx->damage_count -= count;

    ui_unlock(&ctx->lock);

    return count;
}


void ui_clear_component(component_t *component)
{
    if (!component || !component->ctx)
        return;

    if (component->on_clear)
        component->on_clear(component);
}

void ui_enable_component(component_t *component)
{
    if (!component || !component->ctx)
        return;

    ui_context_t *ctx = component->ctx;

    ui_lock(&ctx->lock);

    if (component->enabled) {
        ui_unlock(&ctx->lock);
        return;
    }

    ui_unlock(&ctx->lock);

    
}

void ui_disable_component(component_t *component)
{
    if (!component || !component->ctx)
        return;

    ui_context_t *ctx = component->ctx;
    int x, y, w, h;
    int callback = 0;

    ui_lock(&ctx->lock);

    if (!component->enabled) {
        ui_unlock(&ctx->lock);
        return;
    }

    component->enabled = 0;

    x = component->x;
    y = component->y;
    w = component->w;
    h = component->h;

    if (component->on_disable)
        callback = 1;

    ui_unlock(&ctx->lock);

    if (callback) {
        component->on_disable(component);
        ui_component_release(component);
    }

    ui_invalidate_region(ctx, x, y, w, h);
}

void ui_hide_component(component_t *component)
{
    if (!component || !component->ctx)
        return;

    ui_context_t *ctx = component->ctx;
    int x, y, w, h;

    ui_lock(&ctx->lock);

    if (!component->visible) {
        ui_unlock(&ctx->lock);
        return;
    }

    x = component->x;
    y = component->y;
    w = component->w;
    h = component->h;

    component->visible = 0;
    component->render = 0;

    ui_unlock(&ctx->lock);

    ui_invalidate_region(ctx, x, y, w, h);
}

void ui_show_component(component_t *component)
{
    if (!component || !component->ctx)
        return;

    ui_context_t *ctx = component->ctx;

    ui_lock(&ctx->lock);

    if (component->visible) {
        ui_unlock(&ctx->lock);
        return;
    }

    component->visible = 1;
    component->dirty = 1;
    component->render = 1;

    ui_unlock(&ctx->lock);

}

void ui_destroy_component(component_t *component)
{
    if (!component)
        return;

    ui_context_t *ctx = component->ctx;

    if(!ui_remove_component(ctx, component))
        ui_component_release(component);
}

void ui_destroy_all_components(ui_context_t *ctx)
{
    if (!ctx)
        return;

    while (1) {
        component_t *component = NULL;

        ui_lock(&ctx->lock);

        if (ctx->count > 0)
            component = ctx->components[ctx->count - 1];

        ui_unlock(&ctx->lock);

        if (!component)
            break;

        ui_destroy_component(component);
    }

    ui_lock(&ctx->lock);

    free(ctx->components);
    ctx->components = NULL;
    ctx->count = 0;

    ctx->modal_owner = NULL;
    ctx->sub_modal_owner = NULL;
    ctx->modal_active = 0;

    ui_unlock(&ctx->lock);
}

int ui_add_component(ui_context_t *ctx, component_t *component)
{
    if (!ctx || !component)
        return -1;

    ui_lock(&ctx->lock);

    if (component->ctx && component->ctx != ctx) {
        ui_unlock(&ctx->lock);
        return -1;
    }

    for (int i = 0; i < ctx->count; i++) {
        if (ctx->components[i] == component) {
            ui_unlock(&ctx->lock);
            return -1;
        }
    }

    component_t **new_components = realloc(
        ctx->components,
        (ctx->count + 1) * sizeof(component_t *)
    );

    if (!new_components) {
        ui_unlock(&ctx->lock);
        return -1;
    }

    ctx->components = new_components;

    component->ctx = ctx;

    ctx->components[ctx->count++] = component;

    ui_unlock(&ctx->lock);

    return 0;
}

int ui_remove_component(ui_context_t *ctx, component_t *component)
{
    if (!ctx || !component)
        return -1;

    int found = 0;

    ui_lock(&ctx->lock);

    for (int i = 0; i < ctx->count; i++) {
        if (ctx->components[i] != component)
            continue;

        for (int j = i; j < ctx->count - 1; j++) {
            ctx->components[j] = ctx->components[j + 1];
            ctx->components[j + 1] = NULL;
        }

        ctx->count--;
        found = 1;
        break;
    }

    if (found) {
        if (ctx->modal_owner == component)
            ctx->modal_owner = NULL;

        if (ctx->sub_modal_owner == component)
            ctx->sub_modal_owner = NULL;

        if (!ctx->modal_owner && !ctx->sub_modal_owner)
            ctx->modal_active = 0;
    }

    ui_unlock(&ctx->lock);

    return found ? 0 : -1;
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
        ui_notify_redraw(ctx);

    ui_unlock(&ctx->lock);
}

void ui_event(ui_context_t* ctx, event_t ev)
{
    if (!ctx || ev.type == EVENT_NONE)
        return;

    ui_lock(&ctx->lock);

    if (ctx->modal_active && ctx->modal_owner) {
        if (ctx->modal_owner->on_event)
            ctx->modal_owner->on_event(ctx->modal_owner, ev);
        
        ui_unlock(&ctx->lock);
        return;
    }

    for (int i = ctx->count - 1; i >= 0; i--) {
        component_t* c = ctx->components[i];

        if (!c || !c->on_event)
            continue;

        c->on_event(c, ev);
    }

    ui_unlock(&ctx->lock);
}