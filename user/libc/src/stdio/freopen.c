#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/usyscall.h>

/**
 * freopen - Fecha um fluxo existente e reabre-o redirecionado para outro caminho.
 * @filename: Caminho do novo ficheiro a abrir.
 * @mode:     Modo de acesso (ex: "w", "r", "a+").
 * @fp:       O fluxo FILE existente que será reaproveitado.
 * 
 * Retorna: O mesmo ponteiro fp em caso de sucesso, ou NULL em caso de erro.
 */
FILE *freopen(const char *filename, const char *mode, FILE *fp)
{
    // 1. Validação básica de sanidade
    if (!filename || filename[0] == '\0' || !mode || mode[0] == '\0' || !fp) {
        return NULL;
    }

    // 2. Fecha o descritor de ficheiro atual se ele estiver aberto
    // Nota: Usamos a chamada de sistema close() diretamente ou fclose() se preferires,
    // mas mantemos o objeto fp vivo no Heap para reaproveitamento!
    if (fp->fd >= 0) {
        // Se houver dados pendentes no buffer de escrita, descarrega-os primeiro
        fflush(fp);
        close(fp->fd);
        fp->fd = -1;
    }

    // 3. Traduz a string de modo para as flags da chamada de sistema open()
    int open_flags = 0;
    if (mode[0] == 'r') {
        open_flags = O_RDONLY;
    } else if (mode[0] == 'w') {
        open_flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        open_flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        return NULL;
    }

    // Varre para verificar o modo de atualização '+'
    for (int i = 1; mode[i] != '\0' && i < 4; i++) {
        if (mode[i] == '+') {
            open_flags = (open_flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
            break;
        }
    }

    // 4. Abre o novo ficheiro no Kernel (Ring 3 -> Ring 0)
    int new_fd = open(filename, open_flags);
    if (new_fd < 0) {
        return NULL; // Falha ao abrir o ficheiro alvo
    }

    // 5. Reinicializa os metadados da estrutura FILE existente com o novo estado
    fp->fd = new_fd;
    fp->flags = 0;
    fp->has_ungetc = 0;
    fp->ungetc_buf = 0;

    // Configura as novas máscaras de bits no fluxo baseando-se no modo escolhido
    if (mode[0] == 'r') fp->flags |= _IOREAD;
    else if (mode[0] == 'w') fp->flags |= _IOWRT;
    else if (mode[0] == 'a') fp->flags |= _IOWRT | _IOAPP;

    for (int i = 1; mode[i] != '\0' && i < 4; i++) {
        if (mode[i] == '+') fp->flags |= _IOREAD | _IOWRT;
        else if (mode[i] == 'b') fp->flags |= _IOBIN;
    }

    return fp; // Retorna o mesmo ponteiro estruturado, agora redirecionado!
}