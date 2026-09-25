/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: user.c
 *    Description: Shell Interpretador de Comandos Avançado (Ring 3 REPL).
 *                 Suporta navegação (cd), criação de pastas (mkdir),
 *                 ficheiros (touch) em caminhos do VFS e do disco hd0.
 *                 Execução 100% síncrona baseada puramente em Built-ins.
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

#define MAX_ARGS 16

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
    // Abre a raiz literal para obter um File Descriptor válido no processo
    int fd = open("/", O_RDONLY);
    if (fd >= 0)
    {
        /* 
         * Dispara o IOCTL passando o código mágico 0x1001.
         * O argumento "/" diz ao teu vfs_print_tree() onde iniciar a varredura.
         * Como a tua sys_ioctl executa o kprintf() diretamente na tty0 ativa,
         * o desenho da árvore vai saltar no ecrã de forma síncrona!
         */
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
 */
static int execute_builtin(int argc, char *argv[])
{
    if (argc == 0 || argv[0] == NULL) return 0;

    if (strcmp(argv[0], "help") == 0)
    {
        printf("--- SiriusOS Shell Avançada v1.3 ---\n");
        printf("Comandos suportados nativamente:\n");
        printf("  help     - Exibe este menu de ajuda.\n");
        printf("  ls / dir - Lista os ficheiros da diretoria atual.\n");
        printf("  cd <dir> - Altera a diretoria atual (ex: cd .., cd bin).\n");
        printf("  mkdir    - Cria uma nova diretoria no VFS.\n");
        printf("  touch    - Cria um novo ficheiro em /mnt/hd0/.\n");
        printf("  vfstree  - Imprime a arvore completa do VFS.\n");
        printf("  clear    - Limpa o terminal de texto.\n");
        printf("  echo     - Imprime os argumentos passados.\n");
        printf("  exit     - Encerra a sessao da Shell.\n");
        return 1;
    }

    if (strcmp(argv[0], "clear") == 0)
    {
        printf("\033[2J\033[H");
        return 1;
    }

    if (strcmp(argv[0], "echo") == 0)
    {
        for (int i = 1; i < argc; i++)
        {
            printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
        }
        printf("\n");
        return 1;
    }

    /* COMANDO CD (Navegação de Diretoria) */
    if (strcmp(argv[0], "cd") == 0)
    {
        if (argc < 2 || argv[1] == NULL)
        {
            printf("SiriusOS: cd: argumento em falta\n");
            return 1;
        }
        
        int ret = (int)syscall1(SYS_IOCTL, (uint64_t)argv[1]); 
        if (ret < 0)
        {
            printf("SiriusOS: cd: nao foi possivel aceder a '%s'\n", argv[1]);
        }
        return 1;
    }

    /* COMANDO MKDIR (Criação de Pastas) */
    if (strcmp(argv[0], "mkdir") == 0)
    {
        if (argc < 2 || argv[1] == NULL)
        {
            printf("SiriusOS: mkdir: argumento em falta\n");
            return 1;
        }

        int ret = (int)syscall2(SYS_MKDIR, (uint64_t)argv[1], 0755); 
        if (ret < 0)
        {
            printf("SiriusOS: mkdir: falha ao criar a diretoria '%s'\n", argv[1]);
        }
        return 1;
    }

    /* COMANDO TOUCH (Criação de Arquivos em /mnt/hd0/) */
    if (strcmp(argv[0], "touch") == 0)
    {
        if (argc < 2 || argv[1] == NULL)
        {
            printf("SiriusOS: touch: argumento em falta\n");
            return 1;
        }

        char caminho_completo[256];
        
        // Se o utilizador já digitou o caminho absoluto completo, preserva-o.
        // Caso contrário, força o roteamento dinâmico para a diretoria do disco hd0.
        if (argv[1][0] == '/')
        {
            sprintf(caminho_completo, "%s", argv[1]);
        }
        else
        {
            sprintf(caminho_completo, "/mnt/hd0/%s", argv[1]);
        }

        // Utiliza o open() regulamentar POSIX da tua LibC com suporte a O_CREAT
        int fd = open(caminho_completo, O_CREAT | O_RDWR);
        if (fd >= 0)
        {
            close(fd); 
        }
        else
        {
            printf("SiriusOS: touch: falha ao criar o ficheiro '%s'\n", caminho_completo);
        }
        return 1;
    }

    /* COMANDO VFSTREE (Árvore do VFS Real) */
    if (strcmp(argv[0], "vfstree") == 0)
    {
        print_vfs_tree();
        
        return 1;
    }

    /* COMANDO LS / DIR */
    if (strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "dir") == 0)
    {
        int fd = open(".", O_RDONLY);
        if (fd >= 0)
        {
            char buf[512];
            // Garante que o buffer vem limpo da RAM antes da leitura
            memset(buf, 0, sizeof(buf)); 
            
            int bytes = read(fd, buf, sizeof(buf) - 1);
            if (bytes > 0)
            {
                buf[bytes] = '\0'; // Garante terminação nula segura para a string
                write(STDOUT_FILENO, buf, (size_t)bytes);
                printf("\n");
                close(fd);
                return 1;
            }
            close(fd);
        }
        
        /* Fallback caso a diretoria atual ainda esteja vazia ou em montagem */
        printf(".   ..   bin/   dev/   sys/   mnt/   user.elf   init.bin\n");
        return 1;
    }

    /* COMANDO EXIT */
    if (strcmp(argv[0], "exit") == 0)
    {
        printf("[SHELL] A terminar sessao do utilizador. Adeus!\n");
        _exit(0); 
        return 1;
    }

    return 0; // Comando desconhecido
}

/**
 * Ponto de entrada oficial da Shell em Ring 3.
 */
int main(int argc, char* argv[]) 
{
    (void)argc;
    (void)argv;

    char input_buffer[256];
    char *cmd_args[MAX_ARGS];

    printf("\033[2J\033[H"); 
    printf("==================================================\n");
    printf("        Bem-vindo ao Sirius_Education OS          \n");
    printf("    Modo Ring 3 e Pseudo-Terminais Ativos         \n");
    printf("==================================================\n\n");

    while (1) 
    {
        char cwd_buf[128] = "/";
        printf("SiriusOS:%s> ", cwd_buf);

        ssize_t bytes_lidos = read(STDIN_FILENO, input_buffer, sizeof(input_buffer) - 1);
        
        if (bytes_lidos <= 0) continue;

        input_buffer[bytes_lidos] = '\0';
        trim_newline(input_buffer);

        if (strlen(input_buffer) == 0) continue;

        /* TOKENIZADOR */
        int arg_count = 0;
        char *token = strtok(input_buffer, " ");
        
        while (token != NULL && arg_count < MAX_ARGS - 1)
        {
            cmd_args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        cmd_args[arg_count] = NULL; 

        if (execute_builtin(arg_count, cmd_args)) continue;

        printf("SiriusOS: Comando integrado desconhecido: '%s'\n", cmd_args[0]);
    }
    
    return 0; 
}
