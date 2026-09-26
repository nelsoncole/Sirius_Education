#include <stdio.h>
#include <unistd.h>

/**
 * ftell - Obtém a posição atual do cursor de leitura/escrita num fluxo.
 * @fp:   Ponteiro para a estrutura FILE.
 * 
 * Retorna: A posição atual em bytes a partir do início, ou -1L em caso de erro.
 */
long int ftell(FILE *fp)
{
    // 1. Validação básica de sanidade do ponteiro e do File Descriptor
    if (!fp || fp->fd < 0) {
        return -1L;
    }

    // 2. Consulta a posição atual do cursor no VFS através do lseek.
    // lseek(fd, 0, SEEK_CUR) pergunta ao Kernel: "Em que byte estás agora?"
    // SEEK_CUR costuma ser definido como 1 na unistd.h / stdio.h
    off_t current_pos = lseek(fp->fd, 0, 1);

    if (current_pos == (off_t)-1) {
        fp->flags |= _IOERR; // Sinaliza erro na stream se a syscall falhar
        return -1L;
    }

    // 3. REGRA ISO C: Se houver um caractere retido no buffer do ungetc, 
    // a posição lógica visível pelo utilizador retrocede 1 byte.
    if (fp->has_ungetc) {
        current_pos--;
    }

    return (long int)current_pos;
}
