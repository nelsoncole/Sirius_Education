#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <stddef.h>
// -----------------------------------------------------
// Tipo pthread_t
// Pode ser struct ou ponteiro, dependendo da tua implementação
// -----------------------------------------------------
typedef struct pthread {
    void *p;       // ID ou handle retornado pelo kernel
    void *stack;   // Stack alocada para a thread
    int status;            // THREAD_RUNNING, THREAD_EXIT
    void *return_value;    // valor passado por pthread_exit
    int detachstate;       // joinable ou detached
    unsigned long pid;
} *pthread_t;

// -----------------------------------------------------
// Atributos da thread (por enquanto, vazio)
// -----------------------------------------------------
typedef struct pthread_attr {
    size_t stack_size;   // Tamanho da stack (opcional)
    int detachstate;     // 0 = joinable, 1 = detached
} pthread_attr_t;

typedef void *pthread_addr_t;

// -----------------------------------------------------
// Cria uma thread
// pthread_create(&thread, attr, func, arg)
// -----------------------------------------------------
int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg);

// -----------------------------------------------------
// Termina a thread atual, opcionalmente retornando valor
// -----------------------------------------------------
void pthread_exit(void *retval);

// -----------------------------------------------------
// Aguarda a thread terminar e obtém o valor de retorno
// -----------------------------------------------------
int pthread_join(pthread_t thread, void **retval);


/* -------------------------------------------------------
 * pthread_cancel: força thread a encerrar
 * ------------------------------------------------------- */
int pthread_cancel(pthread_t pd);

// -----------------------------------------------------
// Thread atual
// -----------------------------------------------------
//pthread_t pthread_self(void);

#endif // PTHREAD_H
