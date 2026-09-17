/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: user.c
 *    Description: Aplicação inicial de espaço de utilizador (Ring 3).
 *                 Emite "Hello Ring3!" via Assembly Inline autónomo.
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 15/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

/* Números lógicos das Syscalls definidos no vosso syscall.h */
#include "lib/usyscall.h"
#include "lib/uheap.h"

/**
 * Função autónoma de leitura em Ring 3 usando Assembly Inline.
 */
static inline unsigned long sys_read_inline(int fd, void* buffer, unsigned long size) {
    unsigned long ret;

    /*
     * CONVENÇÃO DE HARDWARE X86_64 PARA SYSCALL:
     * rax = Número da Syscall (SYS_READ = 0)
     * rdi = 1º Argumento (fd)
     * rsi = 2º Argumento (buffer)
     * rdx = 3º Argumento (size)
     */
    __asm__ __volatile__ (
        "syscall"
        : "=a"(ret)
        : "a"((unsigned long)SYS_READ), "D"((unsigned long)fd),
          "S"((unsigned long)buffer), "d"((unsigned long)size)
        : "rcx", "r11", "memory"
    );

    return ret;
}

/**
 * Função autónoma de escrita em Ring 3 usando Assembly Inline.
 */
static inline unsigned long sys_write_inline(int fd, const void* buffer, unsigned long size) {
    unsigned long ret;

    __asm__ __volatile__ (
        "syscall"
        : "=a"(ret)
        : "a"((unsigned long)SYS_WRITE), "D"((unsigned long)fd),
          "S"((unsigned long)buffer), "d"((unsigned long)size)
        : "rcx", "r11", "memory"
    );

    return ret;
}

unsigned long strlen(const char *s) {
    const char *tmp = s;
    while (*tmp != '\0') tmp++;
    return (unsigned long)(tmp - s);
}

/**
 * Ponto de entrada da aplicação Ring 3.
 */
int main(int argc, char* argv[]) {

    // Buffer local na Stack do Ring 3 para capturar a linha digitada
    char input_buffer[256];
    
    const char* prompt = "SiriusOS> ";
    sys_write_inline(1, prompt, strlen(prompt));

    /* 
     * Loop REPL (Read-Eval-Print Loop) básico controlado:
     * Aguarda por dados, lê a linha e imprime de volta em loop.
     */
    while (1) {
        // Bloqueia e aguarda dados do descritor 0 (Teclado/TTY)
        unsigned long bytes_lidos = sys_read_inline(0, input_buffer, sizeof(input_buffer) - 1);
        
        // Garante que, se dados válidos forem lidos, eles serão processados
        if (bytes_lidos > 0 && bytes_lidos != (unsigned long)-1) {
            
            // Imprime exatamente a quantidade de bytes recebida de volta na consola (fd 1)
            sys_write_inline(1, input_buffer, bytes_lidos);
        }

        // Reimprime o prompt do sistema para a próxima interação
        sys_write_inline(1, prompt, strlen(prompt));
    }
    
    return 0; 
}