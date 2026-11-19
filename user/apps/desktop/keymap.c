#include <procyon/procyon.h>

#define CTRL_MASK  0x02
#define ALT_MASK   0x04

/* Máscaras de bits para o campo 'modifiers_state' */
#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_CAPS     (1 << 2)

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
            case 0x0E: return KEY_BACKSPACE;
            case 0x0F: return KEY_TAB;
            case 0x1C: return KEY_ENTER;
            case 0x1D: return KEY_CTRL;
            case 0x2A: return KEY_LSHIFT;
            case 0x36: return KEY_RSHIFT;
            case 0x3A: return KEY_CAPSLOCK;
            case 0x5B: return KEY_WIN;
            case 0x38: return KEY_LALT;
        }
    } else {
        // Scancodes que começam com 0xE0
        switch (scancode) {
            case 0x53: return KEY_DELETE;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
        }
    }
    return 0;
}

int scancode_is_especial(int scancode) {

    switch (scancode) {
        case 0x0E:
        case 0x0F:
        case 0x1C:
        case 0x1D:
        case 0x2A:
        case 0x36:
        case 0x3A:
        case 0x5B:
        case 0x38:
            return 1; // GRUPO: ESPECIAL

        default:
            return 0; // GRUPO: ASCII
    }
}
