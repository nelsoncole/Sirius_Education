#include <pthread.h>
#include <sys/usyscall.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

typedef struct {
    void *(*func)(void *);
    void *arg;
} thread_start_t;

__attribute__((naked)) void thread_trampoline(void) {
    __asm__ __volatile__ (
        "mov 0(%rsp), %rax\n"    // função start_routine
        "mov 8(%rsp), %rdi\n"    // argumento
        "add $16, %rsp\n"        // remove struct da stack (RSP alinhado a 16 bytes)
        "call *%rax\n"           // empurra 8 bytes -> RSP fica a 16n+8 (ABI x86_64)
        "mov %rax, %rdi\n"       // passa o retorno de start_routine para pthread_exit
        "call pthread_exit\n"
    );
}

int pthread_create(pthread_t *pd, const pthread_attr_t *attr,
          void *(*start_routine)(void *), void *arg)
{
    (void)attr;

    if (!pd || !start_routine)
        return -1;

    pthread_t tmp = malloc(sizeof(*tmp));
    if (!tmp)
        return -1;

    // 256 KiB de stack
    void *stack_base = malloc(0x40000);
    if (!stack_base) {
        free(tmp);
        return -1;
    }

    memset(stack_base, 0, 0x40000);

    // Garante alinhamento estrito a 16 bytes no topo da stack
    uintptr_t stack = ((uintptr_t)stack_base + 0x40000) & ~(uintptr_t)0xF;

    // Empilha a struct thread_start_t (16 bytes -> mantém alinhamento a 16 bytes)
    stack -= sizeof(thread_start_t);
    thread_start_t ts = { start_routine, arg };
    memcpy((void*)stack, &ts, sizeof(ts));

    tmp->stack = stack_base;
    tmp->pid = getpid();
    *pd = tmp;

    uint64_t tid = syscall2(
        SYS_PTHREAD_CREATE,
        (uint64_t)(uintptr_t)thread_trampoline,
        (uint64_t)stack
    );

    if ((int64_t)tid <= 0) {
        free(stack_base);
        free(tmp);
        *pd = NULL;
        return -1;
    }

    tmp->p = (void*)(uintptr_t)tid;
    tmp->tid = tid;
    tmp->thread_pointer = syscall1(SYS_THREAD_POINTER, (uint64_t)tid);

    return 0;
}


void pthread_exit(void *retval){
    
    syscall1(SYS_PTHREAD_EXIT, (uint64_t)(uintptr_t)retval);
}


int pthread_join(pthread_t thread, void **retval)
{
    if (!thread)
        return -1;

    uint64_t *target = (uint64_t*)thread->thread_pointer;

    if (!target)
        return -1;

    syscall2(
        SYS_PTHREAD_JOIN,
        (uint64_t)target,
        (uint64_t)retval);

    free(thread->stack);
    free(thread);

    return 0;
}

int pthread_cancel(pthread_t pd) {
    if (!pd || !pd->p)
        return -1;

    // Nota: Em sistemas reais, deve enviar uma syscall para matar a thread no kernel
    // antes de libertar a stack (ex: syscall1(SYS_PTHREAD_CANCEL, pd->tid)).
    pd->p = NULL;

    if (pd->stack) {
        free(pd->stack);
        pd->stack = NULL;
    }

    free(pd);
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr)
{
    (void)attr;

    if (!mutex)
        return -1;

    __atomic_store_n(&mutex->lock, 0, __ATOMIC_RELEASE);

    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (!mutex)
        return -1;

    while (__atomic_exchange_n(
        &mutex->lock,
        1,
        __ATOMIC_ACQUIRE
    ))
    {
        __asm__ volatile("pause");
    }

    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    if (!mutex)
        return -1;

    int expected = 0;

    if (!__atomic_compare_exchange_n(
        &mutex->lock,
        &expected,
        1,
        0,
        __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED
    ))
        return -1;

    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (!mutex)
        return -1;

    if (!__atomic_load_n(&mutex->lock, __ATOMIC_RELAXED))
        return -1;

    __atomic_store_n(&mutex->lock, 0, __ATOMIC_RELEASE);

    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (!mutex)
        return -1;

    if (__atomic_load_n(&mutex->lock, __ATOMIC_ACQUIRE))
        return -1;

    return 0;
}