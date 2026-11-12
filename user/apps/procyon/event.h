#ifndef EVENT_H
#define EVENT_H

typedef struct window_t window_t;

#define MAX_EVENTS 128

typedef enum {
    EVENT_NONE,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_TEXT_INPUT,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_LEFT_DOWN,
    EVENT_MOUSE_RIGHT_DOWN,
    EVENT_MOUSE_MIDDLE_DOWN,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_CLOSE,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_CLIENT_MESSAGE
} event_type_t;

#define KEY_BACKSPACE  8   // '\b' = 0x08
#define KEY_TAB        9   // '\t' = 0x09
#define KEY_ENTER      10  // '\n' = 0x0A
#define KEY_ESCAPE     27  // 0x1B

// Estas não têm ASCII definido e costumam vir como códigos "estendidos"
#define KEY_DELETE     127  // DEL (0x7F) é o código ASCII de delete
#define KEY_LEFT       0x25 // normalmente 0x25 em sistemas Windows (Virtual-Key Code)
#define KEY_RIGHT      0x27 // normalmente 0x27
#define KEY_UP         0x26
#define KEY_DOWN       0x28

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
void handle_mouse_event(int delta_x, int delta_y, int buttons);

#endif