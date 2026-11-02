#include "utf8_decode.h"

int utf8_decode(const char *utf8, uint32_t *codepoint)
{
    if (utf8 == NULL || codepoint == NULL)
        return -1;

    const unsigned char *s = (const unsigned char *)utf8;
    uint32_t cp;
    size_t length;

    if (s[0] == 0)
        return 0;

    if (s[0] <= 0x7F) {
        cp = s[0];
        length = 1;
    } else if (s[0] >= 0xC2 && s[0] <= 0xDF) {
        if (s[1] == 0 || (s[1] & 0xC0) != 0x80)
            return -1;

        cp = ((uint32_t)(s[0] & 0x1F) << 6) |
             (uint32_t)(s[1] & 0x3F);
        length = 2;
    } else if (s[0] >= 0xE0 && s[0] <= 0xEF) {
        if (s[1] == 0 || s[2] == 0 ||
            (s[1] & 0xC0) != 0x80 ||
            (s[2] & 0xC0) != 0x80)
            return -1;

        if ((s[0] == 0xE0 && s[1] < 0xA0) ||
            (s[0] == 0xED && s[1] >= 0xA0))
            return -1;

        cp = ((uint32_t)(s[0] & 0x0F) << 12) |
             ((uint32_t)(s[1] & 0x3F) << 6) |
             (uint32_t)(s[2] & 0x3F);
        length = 3;
    } else if (s[0] >= 0xF0 && s[0] <= 0xF4) {
        if (s[1] == 0 || s[2] == 0 || s[3] == 0 ||
            (s[1] & 0xC0) != 0x80 ||
            (s[2] & 0xC0) != 0x80 ||
            (s[3] & 0xC0) != 0x80)
            return -1;

        if ((s[0] == 0xF0 && s[1] < 0x90) ||
            (s[0] == 0xF4 && s[1] > 0x8F))
            return -1;

        cp = ((uint32_t)(s[0] & 0x07) << 18) |
             ((uint32_t)(s[1] & 0x3F) << 12) |
             ((uint32_t)(s[2] & 0x3F) << 6) |
             (uint32_t)(s[3] & 0x3F);
        length = 4;
    } else {
        return -1;
    }

    *codepoint = cp;
    return (int)length;
}

size_t utf8_decode_string(const char *utf8, uint32_t *output, size_t output_capacity)
{
    if (utf8 == NULL || (output == NULL && output_capacity != 0))
        return 0;

    size_t count = 0;
    const char *p = utf8;

    while (*p != '\0' && count < output_capacity) {
        uint32_t cp;
        int bytes = utf8_decode(p, &cp);

        if (bytes < 1)
            return count;

        output[count++] = cp;
        p += bytes;
    }

    return count;
}