libc/
├── Makefile                     # Script de compilação automatizada da LibC (.a / .so)
├── include/                     # --- CABEÇALHOS PÚBLICOS (API POSIX/ISO C) ---
│   ├── sys/                     # Definições específicas do Sistema Operativo
│   │   ├── types.h              # Tipos primitivos padrão (pid_t, size_t, ssize_t) [pt]
│   │   ├── ioctl.h              # Códigos de controlo de dispositivos (TIOCGPTN, etc.)
│   │   └── syscall.h            # Enumeração unificada dos números das Syscalls do Kernel
│   ├── fcntl.h                  # Flags de controlo e abertura de ficheiros (O_RDWR, O_CREAT) [pt]
│   ├── stdarg.h                 # Suporte a argumentos variáveis (va_list, va_start) para o printf
│   ├── stddef.h                 # Definições padrão comuns (NULL, size_t, offsetof)
│   ├── stdio.h                  # Entrada/Saída padrão (printf, sprintf, fputs) [pt]
│   ├── string.h                 # Manipulação de memória e strings (memcpy, strlen, strcmp)
│   └── unistd.h                 # O coração do POSIX (read, write, close, fork, dup2, execve) [pt]
└── src/                         # --- IMPLEMENTAÇÃO EM C & ASSEMBLY ---
    ├── arch/                    # Código dependente da arquitetura de hardware
    │   └── x86_64/              # Alvo: Intel/AMD 64-bits Long Mode
    │       ├── crt0.asm         # Ponto de entrada real em Ring 3 (Chama o main() e limpa a stack)
    │       └── syscall.asm      # Stubs puros em Assembly para disparar a instrução 'syscall'
    ├── stdio/                   # Implementação do subsistema de E/S Standard
    │   ├── printf.c             # Formatador de alto nível para stdout (Ring 3)
    │   └── sprintf.c            # Formatador de strings em memória
    ├── string/                  # Funções de manipulação de buffers e blocos de texto
    │   ├── memcpy.c             # Cópia otimizada de memória em Ring 3
    │   ├── memset.c             # Preenchimento de buffers na RAM
    │   ├── strcmp.c             # Comparador de strings literais
    │   └── strlen.c             # Contador de comprimento de texto
    └── sys/                     # Implementação das pontas das System Calls
        ├── vfs_syscalls.c       # Wrappers POSIX de arquivos (read, write, close, dup2)
        └── proc_syscalls.c      # Wrappers POSIX de tarefas (fork, execve, getpid, exit)