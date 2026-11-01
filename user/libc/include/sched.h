#ifndef _SCHED_H_
#define _SCHED_H_

/* 
 * Estrutura exigida pelo POSIX e embutida no posix_spawnattr_t.
 * Define os parâmetros de escalonamento que o posix_spawn vai injetar no processo.
 */
struct sched_param {
    int sched_priority;  /* Prioridade do processo (valores maiores = maior prioridade) */
};

#endif
