/* libc/src/stdio/stdio.c */
#include <stdio.h>
#include <unistd.h>

// Instancia as estruturas físicas apontando para 0, 1 e 2
static FILE _stdin  = { .fd = STDIN_FILENO,  .flags = 0 };
static FILE _stdout = { .fd = STDOUT_FILENO, .flags = 0 };
static FILE _stderr = { .fd = STDERR_FILENO, .flags = 0 };

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;
