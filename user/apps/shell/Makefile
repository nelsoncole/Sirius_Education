CC      := gcc
LD      := ld

BASE_DIR    		:= ../../
LIBC_DIR        	:= $(BASE_DIR)libc
LIBC_INC        	:= $(LIBC_DIR)/include
CRT0_DIR        	:= $(LIBC_DIR)/src/arch/x86_64
SYSROOT_SH_APP 		:= $(BASE_DIR)../sysroot/apps/bin/sh
SYSROOT_LS_APP 		:= $(BASE_DIR)../sysroot/apps/bin/ls
SYSROOT_HELP_APP	:= $(BASE_DIR)../sysroot/apps/bin/help
LINKER_SCR  		:= $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding -O2
CFLAGS  += -I$(LIBC_INC)

LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

SH_SRC 		:= sh.c
LS_SRC 		:= ls.c
HELP_SRC 	:= help.c

SH_OBJ 		:= sh.o
LS_OBJ 		:= ls.o
HELP_OBJ 	:= help.o

CRT0    := $(CRT0_DIR)/crt0.o
LIBC_A  := $(LIBC_DIR)/libc.a

# REGRAS DE COMPILAÇÃO
.PHONY: all clean

all: $(SYSROOT_SH_APP) $(SYSROOT_LS_APP) $(SYSROOT_HELP_APP)

$(SYSROOT_SH_APP): $(SH_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_SH_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(SH_OBJ) $(LIBC_A) -o $@

$(SYSROOT_LS_APP): $(LS_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_LS_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(LS_OBJ) $(LIBC_A) -o $@

$(SYSROOT_HELP_APP): $(HELP_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_HELP_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(HELP_OBJ) $(LIBC_A) -o $@

$(SH_OBJ): $(SH_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(LS_OBJ): $(LS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(HELP_OBJ): $(HELP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	@echo "[CLEAN] A remover ficheiros de objeto locais..."
	rm -f $(SH_OBJ) $(LS_OBJ)
