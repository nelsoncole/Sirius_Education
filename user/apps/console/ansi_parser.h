#ifndef ANSI_PARSER_H
#define ANSI_PARSER_H

#include <stdint.h>

typedef enum {
    ANSI_NORMAL,
    ANSI_ESC,
    ANSI_CSI
} ansi_state_t;

typedef struct {
    ansi_state_t state;
    int params[16];
    int param_count;
    int current_param;
    int has_param;
} ansi_parser_t;

void ansi_parser_init(ansi_parser_t *parser);

#endif