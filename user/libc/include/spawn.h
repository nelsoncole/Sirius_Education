#ifndef _SPAWN_H_
#define _SPAWN_H_

#include <sys/types.h>
#include <sched.h>
#include <signal.h>

/* --- Tipos de Dados --- */

/* Estrutura opaca para atributos de criação de processo */
typedef struct {
    short int __flags;
    pid_t __pgrp;
    sigset_t __sd;
    sigset_t __ss;
    struct sched_param __sp;
    int __policy;
    int __pad[16];
} posix_spawnattr_t;

/* Estrutura opaca para ações de ficheiros (redirecionamento, fecho, etc.) */
typedef struct {
    int __allocated;
    int __used;
    void *__actions;
    int __pad[16];
} posix_spawnfile_actions_t;

/* Estrutura opaca para ações de ficheiros (redirecionamento, fecho, etc.) */
typedef struct {
    int __allocated;
    int __used;
    void *__actions;
    int __pad;
} posix_spawn_file_actions_t; /* <-- Corrigido aqui (adicionado o _) */



/* --- Constantes (Flags para posix_spawnattr_setflags) --- */

#define POSIX_SPAWN_RESETIDS      0x01
#define POSIX_SPAWN_SETPGROUP     0x02
#define POSIX_SPAWN_SETSCHEDPARAM 0x04
#define POSIX_SPAWN_SETSCHEDULER  0x08
#define POSIX_SPAWN_SETSIGDEF     0x10
#define POSIX_SPAWN_SETSIGMASK    0x20

/* Funções principais para gerar processos */
int posix_spawn(pid_t * pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const argv[], char *const envp[]);

int posix_spawnp(pid_t *restrict pid, const char *file,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp,
                 char *const argv[], char *const envp[]);


#endif