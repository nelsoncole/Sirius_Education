#include "keymap.h"

// Sem Shift
static const char kbd_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',    // 0x00 - 0x0F
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,     // 0x10 - 0x1F
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,  '\\',       // 0x20 - 0x2F
    'z','x','c','v','b','n','m',',','.','/', 0,   '*', 0,  ' ',       // 0x30 - 0x3F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  '-', 0,  0,  0,  '+',  // 0x40 - 0x4F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,    // 0x50 - 0x5F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,    // 0x60 - 0x6F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0     // 0x70 - 0x7F
};


// Com Shift
static const char kbd_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   // 0x00 - 0x0F
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,               // 0x10 - 0x1F
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,  '|',                   // 0x20 - 0x2F
    'Z','X','C','V','B','N','M','<','>','?', 0,   '*', 0,  ' ',                 // 0x30 - 0x3F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  '_', 0,  0,  0,  '+',           // 0x40 - 0x4F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,              // 0x50 - 0x5F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,              // 0x60 - 0x6F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0               // 0x70 - 0x7F
};

// Com Caps Lock
static const char kbd_map_caps_lock[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',              // 0x00 - 0x0F
    '\t','Q','W','E','R','T','Y','U','I','O','P','[',']','\n', 0,               // 0x10 - 0x1F
    'A','S','D','F','G','H','J','K','L',';','\'','`', 0,  '\\',                 // 0x20 - 0x2F
    'Z','X','C','V','B','N','M',',','.','/', 0,   '*', 0,  ' ',                 // 0x30 - 0x3F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  '-', 0,  0,  0,  '+',           // 0x40 - 0x4F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,              // 0x50 - 0x5F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,              // 0x60 - 0x6F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0               // 0x70 - 0x7F
};

// com shift e Caps Lock
static const char kbd_map_shift_and_caps_lock[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   // 0x00 - 0x0F
    '\t','q','w','e','r','t','y','u','i','o','p','{','}','\n', 0,     // 0x10 - 0x1F
    'a','s','d','f','g','h','j','k','l',':','"','~', 0,  '|',       // 0x20 - 0x2F
    'z','x','c','v','b','n','m','<','>','?', 0,   '*', 0,  ' ',       // 0x30 - 0x3F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  '_', 0,  0,  0,  '+',  // 0x40 - 0x4F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,    // 0x50 - 0x5F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,    // 0x60 - 0x6F
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0     // 0x70 - 0x7F
};

char scancode_to_ascii(int scancode, int mods) {
    if (scancode > 127) return 0;

    // Trata teclas normais
    if((mods & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) && (mods & KBD_MOD_CAPS))
        return kbd_map_shift_and_caps_lock[scancode];
    else if (mods & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT))
        return kbd_map_shift[scancode];
    else if(mods & KBD_MOD_CAPS)
        return kbd_map_caps_lock[scancode];
    else
        return kbd_map[scancode];
}

int scancode_to_key(int scancode, int extended) {
    if (!extended) {
        switch (scancode) {
            case 0x01: return KEY_ESCAPE;
            case 0x0E: return KEY_BACKSPACE;
            case 0x0F: return KEY_TAB;
            case 0x1C: return KEY_ENTER;
            case 0x1D: return KEY_CTRL;
            case 0x2A: return KEY_LSHIFT;
            case 0x36: return KEY_RSHIFT;
            case 0x3A: return KEY_CAPSLOCK;
            case 0x38: return KEY_LALT;

            // Teclas de Função (F1 - F12)
            case 0x3B: return KEY_F1;
            case 0x3C: return KEY_F2;
            case 0x3D: return KEY_F3;
            case 0x3E: return KEY_F4;
            case 0x3F: return KEY_F5;
            case 0x40: return KEY_F6;
            case 0x41: return KEY_F7;
            case 0x42: return KEY_F8;
            case 0x43: return KEY_F9;
            case 0x44: return KEY_F10;
            case 0x57: return KEY_F11;
            case 0x58: return KEY_F12;
        }
    } else {
        // Scancodes que começam com 0xE0
        switch (scancode) {
            case 0x1D: return KEY_CTRL;      // Right Ctrl
            case 0x38: return KEY_RALT;      // Right Alt
            case 0x5B: return KEY_WIN;       // Left Win
            case 0x5C: return KEY_WIN;       // Right Win

            case 0x53: return KEY_DELETE;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;

            case 0x47: return KEY_HOME;
            case 0x4F: return KEY_END;
            case 0x49: return KEY_PAGEUP;
            case 0x51: return KEY_PAGEDOWN;

            // Volume e Multimédia (HP)
            case 0x20: return KEY_VOLUME_MUTE;
            case 0x2E: return KEY_VOLUME_DOWN;
            case 0x30: return KEY_VOLUME_UP;
            case 0x19: return KEY_MEDIA_NEXT;
            case 0x10: return KEY_MEDIA_PREV;
            case 0x24: return KEY_MEDIA_STOP;
            case 0x22: return KEY_MEDIA_PLAY;

        }
    }
    return 0;
}
