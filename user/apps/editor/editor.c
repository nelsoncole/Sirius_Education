#include <stdio.h>
#include <stdlib.h>
#include <procyon/procyon.h>

int main(void)
{
    int socket_fd = procyon_init_client();

    if (socket_fd < 0)
        exit(-1);

    window_t *win = window_create(
        30, 20,
        800, 400,
        "Editor",
        argb(255, 0xCC, 0xCC, 0xCC),
        0xFF000000
    );

    if (!win)
        exit(-2);

    ui_context_t ui = {
        .fd = socket_fd,
        .window = win,
        .components = NULL,
        .count = 0,
        .modal_active = 0,
        .modal_owner = NULL,
        .sub_modal_owner = NULL,
        .lock = 0
    };

    component_t *editbox = create_editbox(&ui, win->client.x, win->client.y + 20, win->client.width, win->client.height-20, 24, EDITBOX_TEXT);
    editbox_data_t *e = (editbox_data_t*) editbox->data;
    e->border_radius = 0;
    e->border_thickness = 0;
    e->ctx->window->bg = argb(255, 245, 245, 245);
    e->scrollbar_bg = e->ctx->window->bg;
    editbox->fg = argb(255, 0, 0 , 0);
    editbox->border_color = e->ctx->window->bg;
    editbox->bg = e->ctx->window->bg;

    editbox_set_multiline(editbox, 1);
    editbox_set_scrollbars(
        editbox,
        1,  // Vertical
        1   // Horizontal
    );

    enviar_janela_ao_servidor(socket_fd, win, 1);

    while (1)
    {
        ui_draw(&ui);
        event_t ev = get_next_event_window(win);
        ui_event(&ui, ev);

        if (ev.type == EVENT_WINDOW_CLOSE){
            break;
        }
    }

    if(enviar_remocao_janela(socket_fd, win) == 0){
        window_destroy(win);
    }

    if (socket_fd >= 0)
        procyon_finish_client(socket_fd);

    return 0;
}