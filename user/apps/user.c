/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: user.c
 *    Description: Shell Interpretador de Comandos Avançado (Ring 3 REPL).
 *                 Suporta comandos Built-ins e carregamento de programas
 *                 externos concorrentes usando fork() e waitpid().
 * 
 *         Author: Nelson Cole
 *   Created Date: 25/09/2026
 * ============================================================================
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

extern int __sys_active_tty();
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    __sys_active_tty(); // define o foco em tty0

    while (1)
    {
        int pid_shell = fork();
        if (pid_shell == 0)
        {
            char *sh_args[] = {"apps/bin/sh", NULL};
            execve(sh_args[0], sh_args, NULL);
            _exit(-1);
        }

        if (pid_shell == -1)
            _exit(-1);

        waitpid(pid_shell, NULL, 0);
    }

    return 0;
}