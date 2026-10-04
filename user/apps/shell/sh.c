/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sh.c
  *    Description: Interpretador de comandos (Shell) portado do xv6 (MIT).
 *                 Modificado para resolver caminhos absolutos preservando
 *                 a integridade do vetor argv e o numero correto de argc.
 * 
 *         Author: Nelson Cole (Portabilidade e Adaptação)
 *   Created Date: 29/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 04/10/2026
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
#include <sys/usyscall.h>

// Representação interna dos nós da árvore sintática (AST)
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

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
 * @brief Executa os comandos internos embutidos na Shell (Built-ins).
 * @return 1 se o comando for um built-in processado, 0 caso contrário.
 */
static int execute_builtin(int argc, char *argv[])
{
    if (argc == 0 || argv[0] == NULL) return 0;

    if (strcmp(argv[0], "cd") == 0)
    {
      const char *target_path = NULL;

      if (argc < 2)
      {
          target_path = "/"; 
      }
      else
      {
          target_path = argv[1];
      }

      if (chdir(target_path) < 0)
      {
        fprintf(stderr, "cd: nao foi possivel aceder a '%s'\n", target_path);
        return -1;
      }

      char resolved_path[MAX_PATH];
      if (getcwd(resolved_path, sizeof(resolved_path)) == NULL)
      {
        fprintf(stderr, "cd: erro ao recuperar a diretoria atual\n");
        return -1;
      }

      return 1;
    }

    if (strcmp(argv[0], "clear") == 0)
    {
        printf("\033[2J\033[H");
        return 1;
    }

    if (strcmp(argv[0], "vfstree") == 0)
    {
      int fd = open("/", O_RDONLY);
      if (fd >= 0)
      {
        syscall3(SYS_IOCTL, (uint64_t)fd, 0x1001, (uint64_t)"/");
        close(fd);
      }
      return 1;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        printf("sh: A encerrar sessao da Shell.\n");
        _exit(0);
    }

    return 0; 
}

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
    
    /* --- INJEÇÃO SEGURA DO CAMINHO ABSOLUTO --- */
    char caminho_absoluto[MAX_PATH];
    memset(caminho_absoluto, 0, MAX_PATH);

    // Se o utilizador já digitou um caminho absoluto, usa-o diretamente
    if (ecmd->argv[0][0] == '/') 
    {
        strncpy(caminho_absoluto, ecmd->argv[0], MAX_PATH - 1);
    } 
    else 
    {
        // Força a pesquisa unificada dentro da pasta bin do sistema
        snprintf(caminho_absoluto, MAX_PATH, "/mnt/hd0/apps/bin/%s", ecmd->argv[0]);
    }

    // Substitui apenas o ponteiro do comando executável preservando os argumentos seguintes
    ecmd->argv[0] = caminho_absoluto;
    /* ------------------------------------------- */
    
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
    
    if(fork1() == 0){
      close(1); 
      if(open(pipe_tmp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644) < 0){
        fprintf(stderr, "sh: falha ao criar buffer temporario do pipe\n");
        exit(-1);
      }
      runcmd(pcmd->left);
    }
    
    wait(NULL); 

    if(fork1() == 0){
      close(0); 
      if(open(pipe_tmp_file, O_RDONLY, 0) < 0){
        fprintf(stderr, "sh: falha ao ler buffer temporario do pipe\n");
        exit(-1);
      }
      runcmd(pcmd->right);
    }
    
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

  if (fgets(buf, nbuf, stdin) == NULL)
    return -1; 
    
  if(buf[0] == 0) 
    return -1;
  
  return 0;
}

int main(void)
{
  static char buf[512];
  struct cmd *parsed_ast;

  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    // O parser processa os tokens puros (separa comandos de argumentos)
    parsed_ast = parsecmd(buf);
    if (parsed_ast == NULL)
      continue;

    if (parsed_ast->type == EXEC)
    {
      struct execcmd *ecmd = (struct execcmd *)parsed_ast;
      int argc = 0;
      while (ecmd->argv[argc] != NULL && argc < MAXARGS)
      {
        argc++;
      }

      // Executa built-ins nativos no processo pai da Shell
      if (execute_builtin(argc, ecmd->argv))
      {
        free(parsed_ast);
        continue; 
      }
    }

    // Passa a árvore sintática intacta para o processo filho
    if (fork1() == 0)
      runcmd(parsed_ast);

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
// MOTOR DO PARSER COMPLETO (PORTADO DO XV6 PARA O SIRIUS OS)
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
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "sh: restos a esquerda: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd* parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
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
  struct cmd *cmd;

  cmd = parseexec(ps, es);
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
      panic("sh: falta o ficheiro para redirecionamento");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT|O_TRUNC, 1);
      break;
    case '+': // >> (Append)
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT, 1); // Ajustável conforme suporte do teu VFS
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
    panic("sh: falta fechar o parentese ')'");
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
  while(!peek(ps, es, "|;&)")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("sh: demasiados argumentos");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd* nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
