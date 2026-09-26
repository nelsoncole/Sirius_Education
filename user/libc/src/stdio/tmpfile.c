#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/usyscall.h>

// Contador estático simples para gerar IDs numéricos incrementais
static volatile uint32_t g_tmpfile_counter = 0;

// Declaração externa para que o compilador saiba onde encontrar 
// a variável de controlo de fim do Heap do processo durante a ligação (Linkage)
extern uint64_t g_uheap_current_end;

/**
 * tmpfile - Cria um ficheiro temporário binário em RAM (RamFS) seguro e oculto.
 * 
 * Retorna: Um ponteiro FILE válido pronto para leitura/escrita, ou NULL em falha.
 */
FILE *tmpfile(void)
{
    char path[64];
    uint32_t current_id;

    // CORREÇÃO 1: Utiliza o Built-in atómico oficial e correto do GCC para ler
    // a variável de forma segura em ambientes concorrentes, ou faz o fallback estável
    current_id = __atomic_load_n(&g_tmpfile_counter, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&g_tmpfile_counter, 1, __ATOMIC_SEQ_CST);

    // Mistura o ID com o endereço do heap para garantir entropia/unicidade extra
    current_id ^= (uint32_t)g_uheap_current_end;

    // Gera um caminho único seguro apontando para o teu RamFS persistente
    // Usamos o prefixo "." para torná-lo um ficheiro oculto nativo no teu VFS
    sprintf(path, "/sys/.tmp_%lx", (unsigned long)current_id);

    // Abre o ficheiro usando o open standard com suporte a O_CREAT e Leitura/Escrita
    int fd = open(path, O_CREAT | O_RDWR);
    if (fd < 0)
    {
        printf("SiriusOS: tmpfile: falha ao alocar espaco em /sys/\n");
        return NULL;
    }

    // REGRA DE OURO POSIX: Faz o unlink imediato do caminho físico!
    // Isto remove a entrada de texto do catálogo de diretórios (o 'ls' não o vê),
    // mas o Kernel mantém os blocos de RAM vivos enquanto o descritor 'fd' estiver aberto!
    unlink(path);

    // Converte o File Descriptor bruto num ponteiro estruturado FILE* da tua libc
    FILE *fp = fdopen(fd, "wb+");
    if (!fp)
    {
        close(fd); // Proteção contra fugas de FD se a conversão falhar
        return NULL;
    }

    return fp;
}