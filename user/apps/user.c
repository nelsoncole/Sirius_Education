/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: user.c
 *    Description: Shell Interpretador de Comandos Avançado (Ring 3 REPL).
 *                 Suporta comandos Built-ins e carregamento de programas
 *                 externos concorrentes usando fork() e waitpid().
 * 
 *         Author: Nelson Cole
 *   Created Date: 25/09/2026
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/usyscall.h>
#include <sys/wait.h>

#define MAX_ARGS 16
#define MAX_LINE 256

struct sys_dirent
{
    uint64_t d_ino;          // Número único do Inode (específico do FS)
    uint64_t d_off;          // Próximo offset (índice seguinte na tabela do VFS)
    unsigned short d_reclen; // Tamanho total desta estrutura nesta iteração (com padding)
    unsigned char d_type;    // Tipo do nó (DT_DIR, DT_REG, etc.)
    char d_name[];           // Nome do elemento terminado em '\0' (tamanho dinâmico)
}__attribute__((packed));

/**
 * @brief Remove os caracteres de quebra de linha (\n ou \r) do final da string.
 */
static void trim_newline(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
        len--;
    }
}

/**
 * @brief Varre e imprime a árvore completa do VFS.
 */
static void print_vfs_tree(void)
{
    int fd = open("/", O_RDONLY);
    if (fd >= 0)
    {
        syscall3(SYS_IOCTL, (uint64_t)fd, 0x1001, (uint64_t)"/");
        close(fd);
    }
    else
    {
        printf("SiriusOS: vfstree: nao foi possivel contactar o VFS Core\n");
    }
}

/**
 * @brief Executa os comandos internos embutidos na Shell (Built-ins).
 * @return 1 se o comando for um built-in processado, 0 caso contrário.
 */
static int execute_builtin(int argc, char *argv[])
{
    if (argc == 0 || argv[0] == NULL) return 0;

    if (strcmp(argv[0], "help") == 0)
    {
        printf("--- SiriusOS Shell Avançada v1.4 ---\n");
        printf("Comandos suportados nativamente:\n");
        printf("  help     - Exibe este menu de ajuda.\n");
        printf("  ls / dir - Lista os ficheiros da diretoria atual.\n");
        printf("  cd <dir> - Altera a diretoria atual (ex: cd .., cd bin).\n");
        printf("  mkdir    - Cria uma nova diretoria no VFS.\n");
        printf("  touch    - Cria um novo ficheiro em /mnt/hd0/.\n");
        printf("  rename   - Renomeia/Move um ficheiro ou pasta.\n");
        printf("  vfstree  - Imprime a arvore completa do VFS.\n");
        printf("  clear    - Limpa o terminal de texto.\n");
        printf("  echo     - Imprime os argumentos passados.\n");
        printf("  exit     - Encerra a sessao da Shell.\n");
        return 1;
    }
    
    if (strcmp(argv[0], "vfstree") == 0)
    {
        print_vfs_tree();
        return 1;
    }


    return 0; // Não é um comando interno
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    char linha[MAX_LINE];
    char *args[MAX_ARGS];

    printf("\033[2J\033[H"); // Limpa o ecrã no arranque
    printf("========================================================\n");
    printf("         SIRIUS OS - Interpretador Nativo REPL          \n");
    printf("========================================================\n\n");

    while (1)
    {
        printf("sirius@user:~$ ");

        /* 1. Captura a linha digitada pelo utilizador */
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            break; 
        }

        trim_newline(linha);
        if (strlen(linha) == 0) continue;

        /* 2. Tokenizador: Divide a string por espaços em argumentos */
        int cmd_argc = 0;
        char *token = strtok(linha, " ");
        while (token != NULL && cmd_argc < (MAX_ARGS - 1))
        {
            args[cmd_argc++] = token;
            token = strtok(NULL, " ");
        }
        args[cmd_argc] = NULL; // O vetor de argumentos POSIX deve terminar em NULL

        if (cmd_argc == 0) continue;

        /* 3. Tenta processar como um comando Built-in interno */
        if (execute_builtin(cmd_argc, args)) {
            continue; 
        }

        /* 
         * ============================================================================
         * 4. MOTOR CONCORRENTE: MÁGICA DO FORK PARA COMANDOS EXTERNOS
         * ============================================================================
         * Se não for um comando interno, a Shell duplica o seu próprio processo
         * para carregar o programa de forma isolada na RAM!
         */
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("SiriusOS: Shell: Falha critica ao disparar fork().\n");
        }
        else if (pid == 0)
        {
            execve(args[0], args, NULL);

            printf("SiriusOS: '%s': comando ou binario nao encontrado.\n", args[0]);
            _exit(127); 
        }
        else
        {
            /* CONTEXTO DO PROCESSO PAI (A SHELL) */
            int status = 0;

            /*
             * A Shell bloqueia e cede o processador atonicamente via sys_waitpid,
             * aguardando que o comando externo conclua antes de libertar o prompt.
             */
            waitpid(pid, &status, 0);
            
        }
    }
    return 0;
}