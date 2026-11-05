#ifndef EVENT_H
#define EVENT_H

typedef struct window_t window_t;

#define MAX_EVENTS 64

typedef enum {
    EVENT_NONE,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_TEXT_INPUT,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    MOUSE_LEFT_DOWN,
    MOUSE_RIGHT_DOWN,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_CLOSE,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_CLIENT_MESSAGE
} event_type_t;

typedef struct {
    event_type_t type;
    unsigned long clock;
    union {
        struct {
            int keycode;    // Código da tecla (ex: ASCII ou scancode)
            int mods;       // Modificadores (Shift, Ctrl, Alt)
        } key;

        struct {
            int x, y;       // Posição do mouse
            int button;     // Botão clicado
        } mouse;

        struct {
            int new_width;
            int new_height;
        } resize;

        struct {
            int codepoint;    // Código da tecla (ex: ASCII ou scancode)
            int mods;         // Modificadores (Shift, Ctrl, Alt)
        } text;
    };
} event_t;


void push_event_to_window(window_t* win, event_t ev);
event_t get_next_event_window(window_t* win);

#endif