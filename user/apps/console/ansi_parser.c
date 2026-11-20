#include "ansi_parser.h"

#include <string.h>

void ansi_parser_init(ansi_parser_t *parser)
{
    if (!parser)
        return;

    memset(parser, 0, sizeof(ansi_parser_t));

    parser->state = ANSI_NORMAL;
    parser->param_count = 0;
    parser->current_param = 0;
    parser->has_param = 0;
}