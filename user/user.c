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
 *        License: MIT
 * ============================================================================
 */

/* Número lógico da Syscall definido no syscall.h */
#define SYS_WRITE 1

/**
 * Função autónoma de escrita em Ring 3 usando Assembly Inline.
 * Comunica diretamente com o syscall_entry_stub do Kernel.
 */
static inline unsigned long sys_write_inline(int fd, const void* buffer, unsigned long size) {
    unsigned long ret;

    /*
     * CONVENÇÃO DE HARDWARE X86_64 PARA SYSCALL:
     * rax = Número da Syscall
     * rdi = 1º Argumento (fd)
     * rsi = 2º Argumento (buffer)
     * rdx = 3º Argumento (size)
     * 
     * O compilador GCC encarrega-se de injetar as variáveis nos registadores
     * corretos através das restrições ("a", "D", "S", "d").
     */
    __asm__ __volatile__ (
        "syscall"
        : "=a"(ret)                                                 // Saída: O retorno do Kernel vem em RAX
        : "a"((unsigned long)SYS_WRITE), "D"((unsigned long)fd),    // Entradas: RAX, RDI
          "S"((unsigned long)buffer), "d"((unsigned long)size)      // Entradas: RSI, RDX
        : "rcx", "r11", "memory"                                    // Clobbers: Syscall destrói RCX e R11
    );

    return ret;
}

unsigned long strlen(const char *s)
{
	char *tmp = (char*)s;
	
	while(*tmp != '\0')tmp++;

	return (unsigned long)(tmp - s);
}

/**
 * Ponto de entrada da aplicação Ring 3 após o crt0.asm preparar a Stack.
 */
int main(int argc, char* argv[]) {
    // Evita avisos de variáveis não utilizadas (boas práticas académicas)
    (void)argc;
    (void)argv;

    const char* mensagem = "Hello Ring3!\n";
    unsigned long size = 13;

    /* 
     * Executa a chamada de sistema de forma direta e independente,
     * enviando a mensagem para o descritor 1 (console padrão do Kernel).
     */
    sys_write_inline(1, mensagem, size);

    return 0; 
}