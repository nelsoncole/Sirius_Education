#include "window.h"
#include "event.h"

static int ringbuffer_push(window_t* win, event_t ev) {
    int next = (win->event_head + 1) % MAX_EVENTS;

    // Cheio se o próximo for igual ao tail
    if (next == win->event_tail) {
        return -1; // Buffer cheio, não insere
    }

    win->event_queue[win->event_head] = ev;
    win->event_head = next;
    return 0; // Sucesso
}

static int ringbuffer_pop(window_t* win, event_t* ev_out) {
    if (win->event_head == win->event_tail) {
        return -1; // Buffer vazio
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