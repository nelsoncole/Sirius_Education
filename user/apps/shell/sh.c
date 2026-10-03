/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sh.c
 *    Description: Interpretador de comandos (Shell) portado do xv6 (MIT).
 *                 Modificado para eliminar o uso de pipe() de hardware,
 *                 utilizando pontes síncronas de ficheiros temporários em disco
 *                 com suporte nativo a write(), read(), fork() e execve().
 * 
 *         Author: Nelson Cole (Portabilidade e Adaptação)
 *   Created Date: 29/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/09/2026
 * 
 *   Source Code: Baseado no ficheiro sh.c do repositório mit-pdos/xv6-public
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

// Representação interna dos nós da árvore sintática (AST)
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  
void panic(char*);
struct cmd *parsecmd(char*);

/**
 * runcmd - Executa o comando destrutivamente na imagem virtual. Nunca retorna.
 */
void runcmd(struct cmd *cmd)
{
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  const char* pipe_tmp_file = "/sh_pipe.tmp";

  if(cmd == 0)
    exit(0);

  switch(cmd->type){
  default:
    panic("runcmd: Tipo de comando desconhecido");
    break;
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);
    
    /* Executa o binário através da chamada de sistema nativa */
    execve(ecmd->argv[0], ecmd->argv, NULL);
    
    // Se o execve falhar e retornar, exibe o erro
    fprintf(stderr, "sh: exec %s falhou\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    
    if(open(rcmd->file, rcmd->mode, 0644) < 0){
      fprintf(stderr, "sh: falha ao redirecionar para %s\n", rcmd->file);
      exit(-1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(NULL); 
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    
    /* 
     * 1. PRIMEIRO ESTÁGIO: Executa o comando da esquerda (Produtor)
     * Desvia o stdout (FD 1) para escrever os dados no ficheiro temporário.
     */
    if(fork1() == 0){
      close(1); 
      if(open(pipe_tmp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644) < 0){
        fprintf(stderr, "sh: falha ao criar buffer temporario do pipe\n");
        exit(-1);
      }
      runcmd(pcmd->left);
    }
    
    /* 
     * SINCRO MANDATÓRIA: O pai aguarda o término completo do comando da esquerda.
     * Isto garante que todos os dados foram persistidos no VFS antes da leitura.
     */
    wait(NULL); 

    /* 
     * 2. SEGUNDO ESTÁGIO: Executa o comando da direita (Consumidor)
     * Desvia o stdin (FD 0) para ler os dados a partir do mesmo ficheiro temporário.
     */
    if(fork1() == 0){
      close(0); 
      if(open(pipe_tmp_file, O_RDONLY, 0) < 0){
        fprintf(stderr, "sh: falha ao ler buffer temporario do pipe\n");
        exit(-1);
      }
      runcmd(pcmd->right);
    }
    
    // O pai aguarda a conclusão do comando da direita antes de ceder o prompt
    wait(NULL);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int getcmd(char *buf, int nbuf)
{
  printf("sirius@sh:~$ ");

  memset(buf, 0, nbuf);
  char  b[256];
  memset(b, 0, 256);

  if (fgets(b, nbuf, stdin) == NULL)
    return -1; // EOF (Ctrl+D)
    
  if(b[0] == 0) 
    return -1;

  sprintf(buf, "apps/bin/%s", b);
  
  return 0;
}

int main(void)
{
  static char buf[100];
  // Loop REPL Central
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      buf[strlen(buf)-1] = 0;  // Corta o \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "sh: cd: nao foi possivel aceder a '%s'\n", buf+3);
      continue;
    }
    
    if(fork1() == 0)
      runcmd(parsecmd(buf));
      
    wait(NULL); 
  }
  return 0;
}

void panic(char *s)
{
  fprintf(stderr, "sh: PANICO: %s\n", s);
  exit(-1);
}

int fork1(void)
{
  int pid = fork();
  if(pid == -1)
    panic("fork falhou catastroficamente");
  return pid;
}

// ============================================================================
// CONSTRUTORES DE NÓS DA AST
// ============================================================================

struct cmd* execcmd(void)
{
  struct execcmd *cmd = malloc(sizeof(*cmd));
  if (!cmd) panic("malloc execcmd");
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd = malloc(sizeof(*cmd));
  if (!cmd) panic("malloc redircmd");
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd = malloc(sizeof(*cmd));
  if (!cmd) panic("malloc pipecmd");
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd = malloc(sizeof(*cmd));
  if (!cmd) panic("malloc listcmd");
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd = malloc(sizeof(*cmd));
  if (!cmd) panic("malloc backcmd");
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}

// ============================================================================
// MOTOR DO PARSER
// ============================================================================

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  int ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd* parsecmd(char *s)
{
  char *es = s + strlen(s);
  struct cmd *cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "sh: excesso de caracteres: %s\n", s);
    panic("erro de sintaxe");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd* parseline(char **ps, char *es)
{
  struct cmd *cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd* parsepipe(char **ps, char *es)
{
  struct cmd *cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("sh: ficheiro em falta no redirecionamento");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT|O_TRUNC, 1);
      break;
    case '+':  
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT|O_APPEND, 1);
      break;
    }
  }
  return cmd;
}

struct cmd* parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("sh: erro de sintaxe - falta )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd* parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("sh: erro de sintaxe em parseexec");

    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
        panic("sh: excesso de argumentos passados ao executavel");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd *nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;
    if (cmd == 0)
        return 0;
    switch (cmd->type)
    {
    case EXEC:
        ecmd = (struct execcmd *)cmd;
        for (i = 0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
        break;
    case REDIR:
        rcmd = (struct redircmd *)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;
    case PIPE:
        pcmd = (struct pipecmd *)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;
    case LIST:
        lcmd = (struct listcmd *)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;
    case BACK:
        bcmd = (struct backcmd *)cmd;
        nulterminate(bcmd->cmd);
        break;
    }
    return cmd;
}