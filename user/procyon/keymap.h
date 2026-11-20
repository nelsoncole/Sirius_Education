#ifndef KEYMAP_H
#define KEYMAP_H

#define CTRL_MASK  0x02
#define ALT_MASK   0x04

/* Máscaras de bits para o campo 'modifiers_state' */
#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_CAPS     (1 << 2)
#define KBD_MOD_RELEASE  (1 << 7) /* Tecla solta (Key Up) */

typedef enum {
    KEY_NONE        = 0,

    // Teclas de controlo ASCII padrão (0 - 127)
    KEY_BACKSPACE   = 8,    // '\b'
    KEY_TAB         = 9,    // '\t'
    KEY_ENTER       = 10,   // '\n'
    KEY_ESCAPE      = 27,   // 0x1B
    KEY_DELETE      = 127,  // 0x7F

    // Teclas especiais e modificadoras (valores >= 256)
    KEY_CTRL        = 256,
    KEY_LSHIFT,
    KEY_RSHIFT,
    KEY_CAPSLOCK,
    KEY_WIN,
    KEY_LALT,
    KEY_RALT,
    KEY_FN,         // Tecla Fn (muito comum em portáteis HP)

    // Setas direcionais
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,

    KEY_HOME,
    KEY_END,
    KEY_PAGEUP,
    KEY_PAGEDOWN,

    // Teclas de função
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    // Controlo de Áudio e Multimédia (HP)
    KEY_VOLUME_MUTE,
    KEY_VOLUME_DOWN,
    KEY_VOLUME_UP,
    KEY_MIC_MUTE,       // Comum nos teclados HP modernos
    KEY_MEDIA_PLAY_PAUSE,
    KEY_MEDIA_STOP,
    KEY_MEDIA_PLAY,
    KEY_MEDIA_NEXT,
    KEY_MEDIA_PREV,

    // Controlo de Sistema e Ecrã (HP)
    KEY_BRIGHTNESS_DOWN,
    KEY_BRIGHTNESS_UP,
    KEY_PROJECTOR_MODE, // Tecla de alternar ecrã (Win+P / F4 na HP)
    KEY_PRINT_SCREEN,
    KEY_AIRPLANE_MODE
} KeyCode;

char scancode_to_ascii(int scancode, int mods);
int scancode_to_key(int scancode, int extended);

#endif