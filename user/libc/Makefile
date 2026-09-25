# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Script de compilação automatizada da LibC estática (libc.a).
#                 Suporta compilação cruzada x86_64 baseada em Syscalls Inline
#                 e no ponto de entrada C Runtime (crt0.asm).
# 
#        Author:  Nelson Cole
#   Created Date: 24/09/2026
# 
#        License: MIT
# ============================================================================

# 1. Definição do Toolchain de Compilação Cruzada
CC      := gcc
ASM     := nasm
AR      := ar
RANLIB  := ranlib

# 2. Flags de Compilação para Espaço de Utilizador Isolado (Ring 3)
CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding -I./include -O2
ASMFLAGS:= -f elf64

# 3. Nome da Biblioteca Alvo de Saída
TARGET  := libc.a

# 4. Mapeamento Automático de Diretorias e Ficheiros de Origem
SRC_DIRS := src/stdio src/string src/ctype src/stdlib src/sys

# Coleta todos os ficheiros .c dentro das pastas especificadas
C_SRCS   := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

# CORREÇÃO: O único ficheiro Assembly autónomo da tua arquitetura
ASM_SRCS := src/arch/x86_64/crt0.asm

# 5. Mapeamento dos Ficheiros de Objeto de Saída (.o)
OBJS     := $(C_SRCS:.c=.o) $(ASM_SRCS:.asm=.o)

# 6. REGRAS PRINCIPAIS DO MAKEFILE
.PHONY: all clean fclean re

all: $(TARGET)

# Regra de Ligação: Agrupa todos os objetos .o dentro do arquivo estático .a
$(TARGET): $(OBJS)
	@echo "[AR] A empacotar a biblioteca estática $(TARGET)..."
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@
	@echo "[SUCCESS] Biblioteca $(TARGET) gerada com sucesso para Ring 3!"

# Regra de Compilação para ficheiros C (.c -> .o)
%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Regra de Compilação para o C Runtime (.asm -> .o)
%.o: %.asm
	@echo "[ASM] $<"
	$(ASM) $(ASMFLAGS) $< -o $@

# Limpeza dos ficheiros intermédios de objeto
clean:
	@echo "[CLEAN] A remover ficheiros de objeto (.o)..."
	rm -f $(OBJS)

# Limpeza total (Objetos e a biblioteca estática final)
fclean: clean
	@echo "[FCLEAN] A remover a biblioteca $(TARGET)..."
	rm -f $(TARGET)

# Força a reconstrução completa do zero
re: fclean all