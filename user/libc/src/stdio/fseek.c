#include <stdio.h>
#include <unistd.h>

/**
 * fseek - Altera a posição do cursor de leitura/escrita num fluxo usando lseek.
 * @fp:         Ponteiro para a estrutura FILE.
 * @num_bytes:  Deslocamento em bytes (positivo ou negativo).
 * @origin:     Ponto de partida: SEEK_SET (0), SEEK_CUR (1) ou SEEK_END (2).
 * 
 * Retorna: 0 em caso de sucesso absoluto, ou -1 em caso de erro.
 */
int fseek(FILE *fp, long num_bytes, int origin)
{
    // 1. Validação básica de sanidade do ponteiro e do File Descriptor
    if (!fp || fp->fd < 0) {
        return -1;
    }

    // 2. Invoca a função lseek nativa da tua unistd.h
    // Ela trata de fazer o chaveamento para o Ring 0 e atualizar o offset no VFS
    off_t ret = lseek(fp->fd, (off_t)num_bytes, origin);
    
    if (ret == (off_t)-1) {
        fp->flags |= _IOERR; // Ativa o indicador de erro no fluxo se a lseek falhar
        return -1;
    }

    // 3. REGRA ISO C: Mover o cursor limpa o buffer do ungetc imediatamente!
    fp->has_ungetc = 0;
    fp->ungetc_buf = 0;

    // 4. Limpa o indicador de Fim de Ficheiro (EOF). 
    // Mover o cursor permite que novas tentativas de leitura ocorram na stream.
    fp->flags &= ~_IOEOF;

    return 0; // Sucesso absoluto
}