/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: proc_syscalls.c
 *    Description: Implementação das chamadas de sistema (Syscalls) de gestão
 *                 de processos, identidade, fluxo de execução e sincronismo.
 *                 Utiliza as macros universais inline de usyscall.h.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 01/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <stdarg.h>
#include <sys/usyscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <spawn.h>
#include <errno.h>
#include <sync/sync.h>


#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 4096
#endif

/**
 * _exit - Termina o processo atual de forma imediata e atómica.
 */
void _exit(int status)
{
    syscall1(SYS_EXIT, (uint64_t)status);

    /* Salvaguarda contra corrupção ou falha catastrófica */
    while (1) 
    {
        __asm__ __volatile__("pause");
    }
}

/**
 * fork - Cria um processo filho duplicando o contexto do processo pai.
 */
pid_t fork(void)
{
    return (pid_t)syscall0(SYS_FORK);
}

/**
 * execve - Substitui o binário do processo atual por um novo executável.
 *          Ponto de entrada nativo da Syscall no Kernel.
 */
int execve(const char *pathname, char *const argv[], char *const envp[])
{
    char new_pathname[MAX_PATH_LEN];
    if (pathname[0] == '/')
    {
        strcpy(new_pathname, pathname);
    }
    else
    {
        char pwd[MAX_PATH_LEN];
        if (getcwd(pwd, MAX_PATH_LEN) == NULL)
        {
            return -1;
        }
        else
        {
            sprintf(new_pathname, "%s/%s", pwd, pathname);
        }
    }

    return (int)syscall3(SYS_EXECVE, (uint64_t)new_pathname, (uint64_t)argv, (uint64_t)envp);
}

/**
 * execv - conveniente para execve omitindo variáveis de ambiente.
 */
int execv(const char *pathname, char *const argv[])
{
    /* Mapeia o vetor de ambiente global padrão (extern char **environ) como nulo */
    return execve(pathname, argv, NULL);
}

/**
 * execl - para executar um binário passando argumentos via Lista Variádica.
 */
int execl(const char *pathname, const char *arg, ...)
{
    va_list args;
    va_start(args, arg);

    /* 1. Conta o número de argumentos para alocar o vetor temporário */
    int count = 0;
    if (arg != NULL) 
    {
        count++;
        const char* tmp;
        while ((tmp = va_arg(args, const char*)) != NULL) 
        {
            count++;
        }
    }
    va_end(args);

    /* 2. Cria o vetor argv baseado no tamanho descoberto (+1 para o terminador NULL) */
    char* argv[count + 1];
    
    va_start(args, arg);
    if (arg != NULL)
    {
        argv[0] = (char*)arg;
        for (int i = 1; i < count; i++)
        {
            argv[i] = va_arg(args, char*);
        }
    }
    argv[count] = NULL; /* Terminador estrito obrigatório */
    va_end(args);

    /* 3. Desvia a execução para o ponto nativo */
    return execve(pathname, argv, NULL);
}

/**
 * posix_spawn - Cria um novo processo e executa um programa especificado,
 *               sem substituir a imagem do processo chamador.
 *
 *               Cria um processo filho através de fork() e executa o novo
 *               binário através de execve(). Caso a execução falhe, o
 *               processo filho termina com código de saída 127.
 *
 * @pid:          Ponteiro para receber o PID do processo criado.
 * @pathname:     Caminho do executável a ser iniciado.
 * @file_actions: Operações de ficheiros a executar antes do execve().
 * @attrp:        Atributos de criação e execução do processo.
 * @argv:         Argumentos da linha de comandos.
 * @envp:         Variáveis de ambiente do novo processo.
 *
 * @return:       0 em caso de sucesso ou código de erro POSIX.
 */
int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const argv[], char *const envp[])
{
    // 1. Validação básica de segurança
    if (path == NULL /*|| argv == NULL || envp == NULL*/) {
        return EINVAL; // Código de erro POSIX para argumento inválido
    }

    char new_pathname[MAX_PATH_LEN];
    
    // 2. Resolução do caminho (Absoluto vs Relativo)
    if (path[0] == '/')
    {
        // Garante que o caminho não ultrapassa o limite do buffer
        if (strlen(path) >= MAX_PATH_LEN) return ENAMETOOLONG;
        strcpy(new_pathname, path);
    }
    else
    {
        char pwd[MAX_PATH_LEN];
        if (getcwd(pwd, MAX_PATH_LEN) == NULL)
        {
            return errno; // Devolve o erro gerado pelo getcwd
        }
        
        // Verifica se o tamanho combinado cabe no buffer (incluindo a '/' e o '\0')
        if ((strlen(pwd) + strlen(path) + 2) > MAX_PATH_LEN) {
            return ENAMETOOLONG;
        }
        sprintf(new_pathname, "%s/%s", pwd, path);
    }

    int ret = (int)syscall6(SYS_PROCESS_SPAWN, 
                            (uint64_t)pid, 
                            (uint64_t)new_pathname, 
                            (uint64_t)file_actions, 
                            (uint64_t)attrp, 
                            (uint64_t)argv, 
                            (uint64_t)envp);

    // Se o retorno do kernel for negativo, transforma no erro POSIX positivo correspondente
    if (ret < 0) {
        return -ret; 
    }

    return 0; // Sucesso
}

int posix_spawnp(pid_t *restrict pid, const char *file,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp,
                 char *const argv[], char *const envp[])
{
    // 1. Validação básica de segurança
    if (file == NULL || argv == NULL || envp == NULL) {
        return EINVAL;
    }

    // 2. Se o nome do ficheiro contiver uma barra '/', ignora o PATH
    // e chama diretamente a posix_spawn padrão.
    if (strchr(file, '/') != NULL) {
        return posix_spawn(pid, file, file_actions, attrp, argv, envp);
    }

    // 3. Procurar a variável de ambiente "PATH=" dentro do array envp
    const char *path_env = NULL;
    for (int i = 0; envp[i] != NULL; i++) {
        if (strncmp(envp[i], "PATH=", 5) == 0) {
            path_env = envp[i] + 5; // Salta o "PATH=" para obter os diretórios
            break;
        }
    }

    // Se o PATH não for encontrado no ambiente fornecido, usa um caminho padrão fallback
    if (path_env == NULL) {
        path_env = "/bin:/usr/bin"; 
    }

    // 4. Fazer o parsing da string PATH (separada por ':')
    char path_buffer[MAX_PATH_LEN];
    const char *start = path_env;
    const char *end;
    int last_error = ENOENT; // Erro padrão caso não encontre em lado nenhum

    while (1) {
        end = strchr(start, ':');
        size_t dir_len = end ? (size_t)(end - start) : strlen(start);

        // Se o diretório for vazio (ex: :: ou PATH começando com :), representa a pasta atual "."
        if (dir_len == 0) {
            path_buffer[0] = '.';
            path_buffer[1] = '\0';
            dir_len = 1;
        } else {
            if (dir_len >= MAX_PATH_LEN) {
                // Salta este diretório se for longo demais para o buffer
                if (!end) break;
                start = end + 1;
                continue;
            }
            strncpy(path_buffer, start, dir_len);
            path_buffer[dir_len] = '\0';
        }

        // Construir o caminho completo: "diretorio/ficheiro"
        char candidate_path[MAX_PATH_LEN];
        if ((dir_len + strlen(file) + 2) <= MAX_PATH_LEN) {
            // Se o diretório já terminar em barra, não adiciona outra (evita de //)
            if (path_buffer[dir_len - 1] == '/') {
                sprintf(candidate_path, "%s%s", path_buffer, file);
            } else {
                sprintf(candidate_path, "%s/%s", path_buffer, file);
            }

            // Tenta fazer o spawn com este caminho candidato
            int status = posix_spawn(pid, candidate_path, file_actions, attrp, argv, envp);
            
            // Se o erro for de ficheiro inexistente (ENOENT) ou diretório inválido (ENOTDIR),
            // continua a procurar nos próximos diretórios do PATH.
            if (status == ENOENT || status == ENOTDIR) {
                last_error = status;
            } else {
                // Se teve sucesso (0) ou falhou por outro motivo crítico (ex: falta de memória ENOMEM,
                // formato executável inválido ENOEXEC), interrompe e devolve o erro imediatamente.
                return status;
            }
        }

        if (!end) {
            break; // Processou o último elemento do PATH
        }
        start = end + 1;
    }

    return last_error; // Retorna ENOENT se varreu todo o PATH e não achou o executável
}


// Dentro de proc_syscalls.c (Ring 3):
pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    return (pid_t)syscall3(SYS_WAITPID, (uint64_t)pid, (uint64_t)wstatus, (uint64_t)options);
}

pid_t wait(int *wstatus)
{
    // Passar -1 diz à tua sys_waitpid para libertar o primeiro zombie que encontrar
    return waitpid(-1, wstatus, 0);
}


/**
 * getpid - Obtém o ID do processo atual.
 */
pid_t getpid(void)
{
    return (pid_t)syscall0(SYS_GETPID);
}

/**
 * getppid - Obtém o ID do processo pai.
 */
pid_t getppid(void)
{
    return (pid_t)syscall0(SYS_GETPPID);
}

/**
 * getuid - Obtém o ID do utilizador real do processo.
 */
uid_t getuid(void)
{
    return (uid_t)syscall0(SYS_GETUID);
}

/**
 * getgid - Obtém o ID do grupo real do processo.
 */
gid_t getgid(void)
{
    return (gid_t)syscall0(SYS_GETGID);
}

/**
 * setuid - Define o ID do utilizador ativo.
 */
int setuid(uid_t uid)
{
    return (int)syscall1(SYS_SETUID, (uint64_t)uid);
}

/**
 * setgid - Define o ID do grupo ativo.
 */
int setgid(gid_t gid)
{
    return (int)syscall1(SYS_SETGID, (uint64_t)gid);
}

/**
 * sleep - Suspende a execução da thread atual por um período em segundos.
 */
unsigned int sleep(unsigned int seconds)
{
    return (unsigned int)syscall1(SYS_SLEEP, (uint64_t)seconds);
}

/**
 * usleep - Suspende a execução da thread atual por um período em microssegundos.
 */
int usleep(unsigned int usec)
{
    return (int)syscall1(SYS_USLEEP, (uint64_t)usec);
}

/**
 * Altera o diretório de trabalho atual do processo.
 * Invoca internamente a syscall sys_chdir.
 */
int chdir(const char *path) 
{
    if (!path) return -1;

    char new_pathname[MAX_PATH_LEN];
    memset(new_pathname, 0, MAX_PATH_LEN);

    if (path[0] == '/')
    {
        strcpy(new_pathname, path);
    }
    else
    {
        /* 1. TRATAMENTO CIRÚRGICO PARA COMPONENTES DE DIRETÓRIA (Localizado na Libc) */
        if (strcmp(path, ".") == 0)
        {
            // "cd ." -> Simplesmente mantém o PWD atual
            if (getcwd(new_pathname, MAX_PATH_LEN) == NULL) return -1;
        }
        else if (strcmp(path, "..") == 0)
        {
            // "cd .." -> Recupera o PWD atual e recua um nível
            if (getcwd(new_pathname, MAX_PATH_LEN) == NULL) return -1;
            
            size_t len = strlen(new_pathname);
            if (len > 1) 
            {
                // Remove barra terminal se existir (ex: "/apps/" -> "/apps")
                if (new_pathname[len - 1] == '/') {
                    new_pathname[len - 1] = '\0';
                }

                // Procura a última barra para truncar o caminho
                char *last_slash = strrchr(new_pathname, '/');
                if (last_slash != NULL) 
                {
                    if (last_slash == new_pathname) 
                    {
                        // Se a última barra for a da raiz (ex: "/apps"), recua para "/"
                        *(last_slash + 1) = '\0';
                    } 
                    else 
                    {
                        // Corta a string na última barra (ex: "/apps/bin" -> "/apps")
                        *last_slash = '\0';
                    }
                }
            }
        }
        else
        {
            // Caminho relativo comum (ex: cd "bin" quando se está em "/apps")
            char pwd[MAX_PATH_LEN];
            if (getcwd(pwd, MAX_PATH_LEN) == NULL)
            {
                return -1;
            }
            else
            {
                sprintf(new_pathname, "%s/%s", pwd, path);
            }
        }
    }
    
    // 2. Invoca a Syscall com o caminho estrito e absoluto já resolvido
    int ret = (int)syscall1(SYS_CHDIR, (uint64_t)new_pathname);
    
    if (ret < 0) {
        return -1;
    }
    
    return 0; // Sucesso
}

/**
 * Lê o diretório de trabalho atual do processo.
 * Invoca internamente a syscall sys_getcwd.
 */
char *getcwd(char *buf, size_t size) 
{
    /* 
     * COMPATIBILIDADE POSIX EXTENDIDA:
     * Se buf for NULL, a especificação dita que a libc deve alocar dinamicamente 
     * o buffer na Heap do utilizador com o tamanho solicitado (ou PATH_MAX).
     */
    char *target_buf = buf;
    int allocated = 0;

    if (!target_buf) {
        size_t alloc_size = (size == 0) ? 256 : size; // Usamos o teu limite de 256 bytes
        target_buf = (char *)malloc(alloc_size);
        if (!target_buf) return NULL;
        size = alloc_size;
        allocated = 1;
    }

    /* Invoca a chamada de sistema especializada sys_getcwd passando o buffer e o limite */
    int ret = (int)syscall2(SYS_GETCWD, (uint64_t)target_buf, (uint64_t)size);

    if (ret < 0) {
        // Se alocámos a memória nesta chamada e o kernel falhou, libertamos para evitar memory leak
        if (allocated) free(target_buf);
        errno = -ret;
        return NULL;
    }

    return target_buf; // Retorna o ponteiro para a string com o caminho absoluto
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset)
{
    uint64_t ret = syscall6(SYS_MMAP, (uint64_t)addr, length, prot, flags, fd, offset);

    if(ret == -1UL) return MAP_FAILED;

    return (void*)ret;
}

void munmap(void *addr, size_t length)
{
  syscall2(SYS_MUNMAP, (uint64_t)addr, length); 
}


void uwait(void *chan) {
    syscall1(SYS_UWAIT, (uint64_t)chan); 
}

void uwakeup(void *chan) {
    syscall1(SYS_UWAKEUP, (uint64_t)chan); 
}

int __sys_active_tty() {
     return(int)syscall0(SYS_ACTIVE_TTY);
}