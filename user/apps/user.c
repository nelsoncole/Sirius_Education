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
#include <sys/wait.h>
#include <spawn.h>
#include <stdio.h>

extern char **environ;


extern int __sys_active_tty();

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    __sys_active_tty(); // define o foco em tty0

    pid_t pid;
    int ret;

    char *argv_sshd[] = {
        "apps/sshd.elf",
        NULL
    };

    ret = posix_spawn(&pid, "apps/sshd.elf", NULL, NULL, argv_sshd, environ);
    if (ret != 0) {
        printf("[sshd] Erro ao executar processo: %d\n", ret);
    }else {
        printf("[sshd] Processo criado: PID %d\n", pid);
    }


    char *argv_s[] = {
        "apps/procyon.elf",
        "--background",
        NULL
    };

    ret = posix_spawn(&pid, "apps/procyon.elf", NULL, NULL, argv_s, environ);
    if (ret != 0) {
        printf("[Procyon] Erro ao executar processo: %d\n", ret);
    }else {
        printf("[Procyon] Processo criado: PID %d\n", pid);
    }

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