#include <pthread.h>
#include <sys/usyscall.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    void *(*func)(void *);
    void *arg;
} thread_start_t;

__attribute__((naked)) void thread_trampoline() {
    __asm__ __volatile__ (
        "mov 0(%rsp), %rax\n"    // função start_routine
        "mov 8(%rsp), %rdi\n"    // argumento
        "add $16, %rsp\n"        // remove struct da stack
        "call *%rax\n"           // chama start_routine(arg)
        "mov $0, %rdi\n"
        "call pthread_exit\n"
    );
}

int pthread_create(pthread_t *pd, const pthread_attr_t *attr,
          void *(*start_routine)(void *), void *arg)
{
    (void)attr;

    // 256 KiB de stack
    void *stack_base = malloc(0x40000);
    if (!stack_base)
        return -1;

    memset(stack_base, 0, 0x40000);

    unsigned long stack = (unsigned long)stack_base + 0x40000;
    if (stack % 0x1000 != 0) {
        // Stack não está alinhada a página → erro ou ajustar
        stack &= ~0xF;  // alinha a 16 bytes, suficiente para ABI SSE/FPU
    }

    // empilha a struct thread_start_t na stack
    stack -= sizeof(thread_start_t);
    thread_start_t ts = { start_routine, arg };
    memcpy((void*)stack, &ts, sizeof(ts));

    // (thread_trampoline),    // entry point
    // (stack),                // RSP inicial
    uint64_t tid = syscall2(
        SYS_PTHREAD_CREATE,
        (uint64_t)(uintptr_t)thread_trampoline,
        (uint64_t)stack
    );


    if (tid <= 0)
        return -1;

    pthread_t tmp = malloc(sizeof(pthread_t));
    if (!tmp)
        return -1;

    tmp->p = (void*)tid;
    tmp->stack = stack_base;
    tmp->pid = tid;
    *pd = tmp;

    return 0;
}


void pthread_exit(void *retval){
    syscall1(SYS_PTHREAD_EXIT, (uint64_t)(uintptr_t)retval);
    for(;;);
}


int pthread_join(pthread_t pd, void **retval){

    (void)pd;
    (void)retval;

    return (int)0; // 0 = sucesso, outro = erro
}

/* -------------------------------------------------------
 * pthread_cancel: força thread a encerrar
 * ------------------------------------------------------- */
int pthread_cancel(pthread_t pd) {
    if (!pd || !pd->p) return -1;


    //pthread_exit();

    // libera stack e struct
    if (pd->stack) free(pd->stack);
    free(pd);

    pd->p = NULL;
    pd->stack = NULL;
    return 0;
}