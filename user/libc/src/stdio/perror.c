#include <stdio.h>
#include <string.h>

// Declaração externa da variável global de erros da tua LibC
extern int errno;

/**
 * perror - Imprime uma mensagem de erro do sistema para o fluxo stderr.
 * @s: String de contexto fornecida pelo utilizador (pode ser NULL ou vazia).
 */
void perror(const char *s)
{
    // 1. Obtém a mensagem de texto descritiva do erro atual através da strerror()
    // Se a tua LibC ainda não tiver strerror, podes usar um fallback temporário.
    const char *err_msg = strerror(errno);
    if (!err_msg) {
        err_msg = "Erro desconhecido";
    }

    // 2. Se o utilizador forneceu uma string válida e não vazia, imprime-a primeiro
    if (s && *s != '\0') {
        fputs(s, stderr);
        fputs(": ", stderr);
    }

    // 3. Imprime a mensagem de erro correspondente ao errno atual e salta a linha
    fputs(err_msg, stderr);
    fputc('\n', stderr);
    
    // 4. Garante que os bytes são descarregados imediatamente na consola tty0
    fflush(stderr);
}