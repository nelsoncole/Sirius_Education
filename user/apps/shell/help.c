#include <stdio.h>
#include <string.h>

void menu_geral(void) {
    printf("\n--- SiriusOS Shell Avançada v1.0 ---\n");
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
    printf("\nDica: Digite 'help <comando>' para ver a sintaxe (ex: help cd).\n\n");
}


int main(int argc, char *argv[]) {
    // Se o utilizador digitou apenas "help", exibe o menu geral
    if (argc < 2) {
        menu_geral();
        return 0;
    }

    // Captura o argumento do comando procurado (ex: "help cd" -> argv[1] é "cd")
    const char *cmd = argv[1];

    if (strcmp(cmd, "cd") == 0) {
        printf("\n[Comando: cd]\nSintaxe: cd <diretoria>\nUso: Altera o caminho de trabalho atual do processo.\n\n");
    } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        printf("\n[Comando: ls / dir]\nSintaxe: ls [caminho]\nUso: Lista o conteúdo da diretoria no VFS.\n\n");
    } else if (strcmp(cmd, "mkdir") == 0) {
        printf("\n[Comando: mkdir]\nSintaxe: mkdir <nome_da_pasta>\nUso: Cria um novo nó de diretório no VFS.\n\n");
    } else if (strcmp(cmd, "touch") == 0) {
        printf("\n[Comando: touch]\nSintaxe: touch <nome_do_arquivo>\nUso: Inicializa um ficheiro regular vazio.\n\n");
    } else if (strcmp(cmd, "rename") == 0) {
        printf("\n[Comando: rename]\nSintaxe: rename <origem> <destino>\nUso: Modifica o caminho/nome de um arquivo.\n\n");
    } else if (strcmp(cmd, "vfstree") == 0) {
        printf("\n[Comando: vfstree]\nSintaxe: vfstree\nUso: Imprime o grafo hierárquico estrutural do VFS.\n\n");
    } else if (strcmp(cmd, "clear") == 0) {
        printf("\n[Comando: clear]\nSintaxe: clear\nUso: Envia o comando ANSI para limpar o buffer de ecrã.\n\n");
    } else if (strcmp(cmd, "echo") == 0) {
        printf("\n[Comando: echo]\nSintaxe: echo [texto]\nUso: Duplica os argumentos textuais no stdout.\n\n");
    } else if (strcmp(cmd, "exit") == 0) {
        printf("\n[Comando: exit]\nSintaxe: exit\nUso: Mata o processo sh atual devolvendo o controlo ao init.\n\n");
    } else if (strcmp(cmd, "help") == 0) {
        printf("\n[Comando: help]\nSintaxe: help [comando_alvo]\nUso: Exibe a ajuda integrada do sistema.\n\n");
    } else {
        printf("help: O comando '%s' não é reconhecido pelo SiriusOS.\n", cmd);
        return 1;
    }

    return 0;
}