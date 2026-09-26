#include <stdio.h>
#include <unistd.h>
#include <sys/usyscall.h>

/**
 * fflush - Descarrega os buffers de um fluxo para o armazenamento físico.
 * @fp:     Ponteiro para a estrutura FILE, ou NULL para limpar todos os fluxos.
 * 
 * Retorna: 0 em caso de sucesso absoluto, ou EOF se ocorrer um erro.
 */
int fflush(FILE *fp) {
    // 1. Cenário Canónico POSIX: Se fp for NULL, limpa todos os buffers abertos do processo
    if (fp == NULL) {
        // Invoca a syscall global SYS_FLUSH passando 0. O teu Kernel interpreta isto
        // como uma ordem para sincronizar todos os blocos pendentes no VFS/Drivers.
        int global_ret = (int)syscall1(SYS_FLUSH, 0);
        return (global_ret >= 0) ? 0 : EOF;
    }

    // 2. Se o fluxo for de Leitura pura, o fflush limpa apenas caracteres pendentes do ungetc
    if (fp->flags & _IOREAD) {
        fp->has_ungetc = 0;
        fp->ungetc_buf = 0;
    }

    // 3. Verifica se o fluxo tem o bit de escrita ativo e possui um File Descriptor válido
    if ((fp->flags & _IOWRT) && fp->fd >= 0) {
        
        // Se adicionares buffers de escrita na tua struct FILE mais tarde (ex: fp->buffer),
        // deves descarregá-los aqui através de chamadas consecutivas à sys_write().
        
        // Invoca a chamada de sistema SYS_FLUSH passando o descritor do ficheiro alvo
        int ret = (int)syscall1(SYS_FLUSH, (uint64_t)fp->fd);
        if (ret < 0) {
            fp->flags |= _IOERR; // Ativa a máscara de erro se o hardware falhar
            return EOF;
        }
    }

    return 0; // Sincronização síncrona bem-sucedida
}