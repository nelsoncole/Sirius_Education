# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Script de compilação automatizada exclusivo do daemon sshd.elf.
#                 Liga estaticamente o código em Ring 3 com a libc.a do sistema 
#                 e o motor criptográfico de arquivo único TLSe (Amálgama).
# 
#        Author:  Nelson Cole
#   Created Date: 01/10/2026
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
TLSE_DIR    := $(BASE_DIR)lib/tlse
SYSROOT_APP := $(BASE_DIR)../sysroot/apps/sshd.elf
LINKER_SCR  := $(BASE_DIR)libc/user_x86_64.ld

# 3. Flags de Compilação para Espaço de Utilizador Isolado com TLSe
CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding
CFLAGS  += -DTLS_AMALGAMATION
CFLAGS  += -I$(LIBC_INC) -I$(TLSE_DIR)

# Flags do Linkeditor para gerar o executável final em Ring 3
LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

# 4. Mapeamento de Ficheiros de Origem e Objetos
SSHD_SRC := sshd.c
TLSE_SRC := $(TLSE_DIR)/tlse.c

CRT0    := $(CRT0_DIR)/crt0.o
SSHD_OBJ := sshd.o
# CORREÇÃO: Removemos o libtomcrypt.o para evitar colisões e múltiplas definições
TLSE_OBJ := $(TLSE_DIR)/tlse.o

# Biblioteca estática gerada pelo teu Makefile base
LIBC_A   := $(LIBC_DIR)/libc.a

# 5. REGRAS DE COMPILAÇÃO
.PHONY: all clean

all: $(SYSROOT_APP)

# Executa o GCC em tempo de processamento do Makefile para achar a libgcc.a correta
LIBGCC := $(shell $(CC) -m64 -print-libgcc-file-name)

# Atualiza a regra de ligação para incluir a libgcc.a
$(SYSROOT_APP): $(SSHD_OBJ) $(TLSE_OBJ) $(LIBC_A) $(CRT0)
	@echo "[LD] A ligar o executável estático $@..."
	@mkdir -p $(dir $(SYSROOT_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(SSHD_OBJ) $(TLSE_OBJ) $(LIBC_A) $(LIBGCC) -o $@
	@echo "[SUCCESS] daemon sshd.elf gerado com sucesso para o Sirius OS!"

# Compilação do código do daemon seguro
$(SSHD_OBJ): $(SSHD_SRC)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# CORREÇÃO: Regra específica para compilar o tlse.c contendo a amálgama da libtomcrypt
$(TLSE_DIR)/tlse.o: $(TLSE_DIR)/tlse.c
	@echo "[CC] $< (TLSe Amalgamation)"
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza local dos ficheiros intermédios
clean:
	@echo "[CLEAN] A remover ficheiros de objeto locais..."
	rm -f $(SSHD_OBJ) $(TLSE_OBJ)
