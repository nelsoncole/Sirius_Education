#include "window.h"
#include "event.h"
#include "layer.h"
#include "compositor.h"

#include <sync/sync.h>

static int ringbuffer_push(window_t* win, event_t ev) {
    int next = (win->event_head + 1) % MAX_EVENTS;

    // Cheio se o próximo for igual ao tail
    if (next == win->event_tail) {
        return -1; // Buffer cheio, não insere
    }

    win->event_queue[win->event_head] = ev;
    win->event_head = next;

    uwakeup(&win->wait);

    return 0; // Sucesso
}

static int ringbuffer_pop(window_t* win, event_t* ev_out) {
    while (win->event_head == win->event_tail) {
        //return -1; // Buffer vazio
        __asm__ __volatile__("pause" ::: "memory");
        uwait(&win->wait);
    }

    *ev_out = win->event_queue[win->event_tail];
    win->event_tail = (win->event_tail + 1) % MAX_EVENTS;
    return 0;
}

void push_event_to_window(window_t* win, event_t ev) {
    if (win) {
        ringbuffer_push(win, ev);
    }
}

event_t get_next_event_window(window_t* win) {
    event_t ev = { .type = EVENT_NONE };
    if( ringbuffer_pop(win, &ev)) {
        // Buffer vazio
    }
    return ev;
}

void handle_mouse_event(int delta_x, int delta_y, int buttons)
{
    int focused_closed = 0;
    int dragging = 0;

    co_lock(&compositor_lock);

    window_t* win_focused = get_focused_window();

    if (g_focused_window && g_focused_window->dragging)
    {
        if (!(buttons & 0x01))
        {
            g_focused_window->dragging = 0;
        }
        else
        {
            int s_x = delta_x - g_focused_window->drag_offset_x;
            int s_y = delta_y - g_focused_window->drag_offset_y;

            g_focused_window->x = (s_x < 0) ? 0 : s_x;
            g_focused_window->y = (s_y < 0) ? 0 : s_y;
        }

        dragging = g_focused_window->dragging;
    }

    if (!dragging)
    {
        for (int i = 2; i >= 0; i--)
        {
            for (int j = layer_counts[i] - 1; j >= 0; j--)
            {
                window_t* win = layers[i][j];

                if (!win->visible)
                    continue;

                if (delta_x >= win->x &&
                    delta_x < win->x + win->width &&
                    delta_y >= win->y &&
                    delta_y < win->y + win->height)
                {
                    if (g_focused_window)
                    {
                        g_focused_window->last_mouse_x = delta_x;
                        g_focused_window->last_mouse_y = delta_y;
                    }

                    window_handle_mouse(win, i, delta_x, delta_y, buttons);
                    focused_closed = 1;
                    break;
                }
            }

            if (focused_closed)
                break;
        }
    }

    if (!g_focused_window || dragging) {
        co_unlock(&compositor_lock);
        return;
    }

    if (win_focused == g_focused_window || g_focused_window->type != 1)
    {
        event_t ev_tmp = {0};

        if (buttons & 0x01)
        {
            ev_tmp.type = EVENT_MOUSE_LEFT_DOWN;
        }
        else if (buttons & 0x02)
        {
            ev_tmp.type = EVENT_MOUSE_RIGHT_DOWN;
        }
        else if (buttons & 0x04)
        {
            ev_tmp.type = EVENT_MOUSE_MIDDLE_DOWN;
        }
        else
        {
            ev_tmp.type = EVENT_MOUSE_UP;
        }

        ev_tmp.clock = 0;
        ev_tmp.mouse.x = delta_x - g_focused_window->x;
        ev_tmp.mouse.y = delta_y - g_focused_window->y;
        ev_tmp.mouse.button = buttons;

        push_event_to_window(g_focused_window, ev_tmp);
    }

    co_unlock(&compositor_lock);
}