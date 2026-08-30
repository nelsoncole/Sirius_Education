# 🌌 Sirius_Education OS

O **Sirius_Education** é um sistema operativo didático e modular desenvolvido para a arquitetura `x86_64`. O projeto foi desenhado com foco na clareza estrutural, separando de forma estrita o código genérico do kernel, as abstrações de hardware (`arch`), drivers e módulos dinâmicos (LKM). 

O sistema suporta arranque moderno via **UEFI** através do ambiente estruturado em `sysroot`.

---

## 📂 Arquitetura do Projeto

A árvore de diretórios segue os padrões de organização de kernels modernos e sistemas POSIX:

```text
Sirius_Education/
│
├── config/                  # Configurações de build (.config, Kconfig)
│   └── kernel.config
│
├── scripts/                 # Scripts de automação, linter e linker scripts
│   └── linker/
│       └── x86_64.ld        # Script de ligação para o Kernel x86_64
│
├── include/                 # Cabeçalhos globais e públicos
│   ├── uapi/                # User Space API (usado pelas syscalls e programas)
|   |   └── sirius/
│   └── kernel/              # Cabeçalhos globais do Kernel
│
├── src/                     # Código-fonte do Kernel e subsistemas estruturais
│   ├── boot/                # Configurações de boot, ficheiros EFI e instalações
|   |
│   ├── kernel/              # Core independente de arquitetura
│   │   ├── core/            # Inicialização geral (kernelmain.c, panic.c)
│   │   ├── mm/              # Gestão de memória genérica (PMM, Heap)
│   │   ├── sched/           # Escalonador e controlo de processos
│   │   ├── ipc/             # Comunicação entre processos (Pipes, Sinais)
│   │   └── syscall/         # Interface de chamadas de sistema
|   |
│   ├── arch/                # Código estritamente dependente de hardware
│   │   └── x86_64/          # Contexto específico para Intel/AMD 64-bits (GDT, IDT, Paginação)
│   │       ├── boot/        # Inicialização específica da CPU (trampoline, etc)
│   │       ├── cpu/         # GDT, IDT, ISRs, IRQs, Controlo de Registos
│   │       ├── mm/          # Paginação específica (PML4, tabelas de páginas x86) e VMM
│   │       └── kapi/        # Abstração de hardware para o kernel genérico
|   |
│   ├── drivers/             # Controladores de hardware embutidos (Char, Block, Net, Bus)
│   │   ├── bus/             # PCI, USB Core, ACPI
│   │   ├── char/            # Teclado (input), Displays de texto, Serial (UART)
│   │   ├── block/           # Storage (IDE, AHCI, Ramdisk)
│   │   ├── net/             # Placas de rede (e1000, rtl8139)
│   │   └── video/           # Displays
|   |
│   ├── fs/                  # Subsistema de ficheiros (VFS, FAT, NTFS)
│   │   ├── vfs/             # Virtual File System Core
│   │   ├── fat/             # Implementação FAT12/16/32
│   │   └── ntfs/            # Implementação NTFS
|   |
│   ├── kmods/               # Motor de carregamento de Módulos Dinâmicos (LKM)
│   │   ├── loader/          # Carregador ELF de módulos para o kernel
│   │   └── manager/         # Gestão de símbolos e dependências
|   |
│   └── lib/                 # Biblioteca interna do kernel (libk - kprintf, string)
│       └── kprintf.c, string.c, bitmap.c
│
├── mods/                    # Módulos/Drivers dinâmicos compilados à parte (.ko)
│   ├── sample_mod/
│   └── build/
|   |
├── build/                   # Ficheiros de objetos temporários (.o, .d) - [Esvaziado no clean]
├── sysroot/                 # Árvore do sistema de ficheiros final (Gera a imagem ISO)
│
├── Makefile                 # Sistema de build automatizado
└── README.md                # Documentação principal
```

---

## 🛠️ Pré-requisitos

Para compilar e testar o sistema operativo, necessitas das seguintes ferramentas instaladas no teu ambiente Linux (Ubuntu/Debian recomendado):

```bash
sudo apt update
sudo apt install build-essential gcc-multilib mtools xorriso qemu-system-x86 nasm
```

Se estiveres a compilar a partir de outra arquitetura, recomenda-se a utilização de um **Cross-Compiler** (`x86_64-elf-gcc`).

---

## 🚀 Como Compilar e Executar

O ciclo de desenvolvimento é totalmente gerido pelo `Makefile` principal.

### 1. Compilar o Sistema
Para compilar o kernel, drivers, módulos e preparar a estrutura dentro do `sysroot`:
```bash
make
```

### 2. Executar no QEMU
Para lançar o emulador carregando o ambiente contido em `sysroot` (suportando a diretoria `/EFI/BOOT/BOOTX64.EFI` para UEFI):
```bash
make run
```

### 3. Limpar o Ambiente
Para esvaziar as pastas de compilação temporárias e remover as imagens geradas **sem apagar a árvore estrutural** do projeto:
```bash
make clean
```

---

## 🧠 Fluxo de Arranque (Boot Flow)

1. **Firmware (UEFI):** Lê a partição FAT32 mapeada em `sysroot/` e executa o binário em `EFI/BOOT/BOOTX64.EFI`.
2. **Kernel Entry (`src/arch/x86_64/boot/entry.asm`):** Configura o estado inicial do CPU de 64-bits, define a paginação fundamental (PML4) e a Stack do kernel.
3. **Kernel Core (`src/kernel/core/kernelmain.c`):** O ponto de entrada C (`kernel_main`) assume o controlo, inicializando a memória (`mm`), o escalonador (`sched`), o sistema de ficheiros (`vfs`) e os drivers essenciais.

---
⭐ *Sirius_Education - Desenvolvido para fins educacionais e estudo de sistemas monolíticos modulares.*
