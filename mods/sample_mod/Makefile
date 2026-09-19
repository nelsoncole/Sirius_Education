# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Sistema de build para compilação de módulos dinâmicos (LKM).
#                 Gera código x86_64 relocável adequado para o espaço de Kernel.
# 
#         Author: Nelson Cole
#   Created Date: 19/09/2026
# ============================================================================

CC = gcc
LD = ld

# Diretórios Globais do Projeto
ROOT_DIR = ../..
INCLUDE_DIR = $(ROOT_DIR)/include
BUILD_DIR = $(ROOT_DIR)/mods/build

# Flags de Compilação estritas para o Kernel Ring 0 (Incluindo -m64)
CFLAGS = -m64 -Wall -Wextra -nostdinc -ffreestanding \
         -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-avx \
         -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -O2 \
         -I$(INCLUDE_DIR)

# Flags do Linker para gerar um arquivo binário ELF Relocável
LDFLAGS = -m elf_x86_64 -r

TARGET = $(BUILD_DIR)/sample_mod.ko
OBJ = main.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(OBJ) -o $@
	@echo "[LKM] Modulo compilado com sucesso: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o