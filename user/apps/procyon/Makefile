# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Script de compilação automatizada exclusivo do daemon procyon.elf.
# 
#        Author:  Nelson Cole
#   Created Date: 01/11/2026
# 
#        License: MIT
# ============================================================================

# 1. Definição do Toolchain (Alinhado com a tua libc)
CC      := gcc
LD      := ld

# 2. Caminhos Estruturais do Sirius_Education
BASE_DIR    := ../../
LIBC_DIR    := $(BASE_DIR)libc
LIBC_INC    := $(LIBC_DIR)/include
CRT0_DIR    := $(LIBC_DIR)/src/arch/x86_64
SYSROOT_APP := $(BASE_DIR)../sysroot/apps/procyon.elf
LINKER_SCR  := $(BASE_DIR)libc/user_x86_64.ld

# 3. Flags de Compilação para Espaço de Utilizador Isolado com TLSe
CFLAGS  := -Wall -Wextra -m64 -O2 -nostdinc -ffreestanding
CFLAGS  += -I$(LIBC_INC)

# Flags do Linkeditor para gerar o executável final em Ring 3
LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

# 4. Mapeamento de Ficheiros de Origem e Objetos
PROCYON_SRC := procyon.c

CRT0    := $(CRT0_DIR)/crt0.o
PROCYON_OBJ := procyon.o

# Biblioteca estática gerada pelo teu Makefile base
LIBC_A   := $(LIBC_DIR)/libc.a

# 5. REGRAS DE COMPILAÇÃO
.PHONY: all clean

all: $(SYSROOT_APP)

# Atualiza a regra de ligação para incluir a libgcc.a
$(SYSROOT_APP): $(PROCYON_OBJ) $(LIBC_A) $(CRT0)
	@echo "[LD] A ligar o executável estático $@..."
	@mkdir -p $(dir $(SYSROOT_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(PROCYON_OBJ) $(LIBC_A) -o $@
	@echo "[SUCCESS] daemon procyon.elf gerado com sucesso para o Sirius OS!"

# Compilação do código do daemon seguro
$(PROCYON_OBJ): $(PROCYON_SRC)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza local dos ficheiros intermédios
clean:
	@echo "[CLEAN] A remover ficheiros de objeto locais..."
	rm -f $(PROCYON_OBJ)
