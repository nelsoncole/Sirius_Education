/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stdio.h
 *    Description: Cabeçalho padrão de Entrada/Saída (Standard I/O) para Ring 3.
 *                 Em conformidade com o padrão ISO C e POSIX.
 * 
 *        Author:  Nelson Cole
 *  Created Date: 24/09/2026
 * ============================================================================
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>
#include <stddef.h>


#ifndef NULL
#define NULL ((void *)0)
#endif

// Constantes de limites regulamentares do sistema
#define FOPEN_MAX       32
#define FILENAME_MAX    96
#define L_tmpnam        FILENAME_MAX
#define TMP_MAX         32767

#define EOF             (-1)

// Máscaras de controlo de modo de acesso (usadas pelo fdopen)
#define _IOREAD         0x0001  // Permite leitura
#define _IOWRT          0x0002  // Permite escrita
#define _IOBIN          0x0004  // Modo binário (ex: 'b')
#define _IOAPP          0x0008  // Modo Append (ex: 'a')

// Máscaras de controlo de estado do fluxo FILE
#define _IOEOF          0x0010  // Indicador de Fim de Ficheiro (End-of-File)
#define _IOERR          0x0020  // Indicador de Erro de E/S

/* ISO C: Estrutura que encapsula o File Descriptor */
typedef struct _IO_FILE {
    int fd;
    int flags;
    int ungetc_buf;     // Guarda o caractere devolvido via ungetc
    int has_ungetc;     // Flag booleana (0 ou 1)
} FILE;

// Mapeamento dos fluxos padrão da Consola (TTY0)
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Funções Principais de Formatação e Parsing */
int printf(const char *format, ...);
int vprintf(const char *format, va_list arg);
int sprintf(char *str, const char *format, ...);
int vsprintf(char *str, const char *format, va_list arg);
int vsnprintf(char *buf, size_t max_len, const char *format, va_list ap);
int vfprintf(FILE * fp,const char * fmt, va_list args);
int fprintf(FILE *fp, const char *fmt, ...);
int vfscanf(FILE * fp,const char * fmt, va_list ap);
int fscanf(FILE * fp, const char * fmt, ...);
int snprintf(char *s, size_t n,const char * fmt, ...);
int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *s,const char *fmt, ...);

/* Funções de E/S de Caracteres e Strings Base de stdio */
int putchar(int c);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
char *fgets (char *str,int length,FILE *fp);
int putc (int ch, FILE *fp);
int getc (FILE *fp);
int fputc (int ch, FILE *fp);
int fgetc (FILE *fp);
FILE *fdopen(int fd, const char *mode);
FILE *fopen(const char *filename,const char *mode);
int fclose (FILE *fp);
int fflush(FILE *fp);
size_t fread (void *buffer, size_t size, size_t count, FILE *fp);
size_t fwrite (const void *buffer, size_t size, size_t count, FILE *fp);
int remove (const char *path);
void rewind(FILE *fp);
int fseek (FILE *fp, long num_bytes, int origin );
int feof (FILE *fp);
long int ftell(FILE *fp);



int scanf(char *fmt, ...);
int getchar ();
int ungetc(int c, FILE *fp);
char *tmpnam(char *s);
int rename(const char *old, const char *new);
void perror(const char *s);

extern FILE *freopen(const char *filename, const char *mode, FILE *fp);
extern int ferror (FILE *fp );
extern int setvbuf(FILE * stream, char * buf, int mode, size_t size);
extern void clearerr(FILE *stream);
extern FILE *tmpfile(void);

#endif /* _STDIO_H */