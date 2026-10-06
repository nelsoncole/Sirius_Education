CC      := gcc
LD      := ld

BASE_DIR    		:= ../../
LIBC_DIR        	:= $(BASE_DIR)libc
LIBC_INC        	:= $(LIBC_DIR)/include
CRT0_DIR        	:= $(LIBC_DIR)/src/arch/x86_64

# DESTINOS NO SYSROOT
SYSROOT_SH_APP 		:= $(BASE_DIR)../sysroot/apps/bin/sh
SYSROOT_LS_APP 		:= $(BASE_DIR)../sysroot/apps/bin/ls
SYSROOT_HELP_APP	:= $(BASE_DIR)../sysroot/apps/bin/help
SYSROOT_MKDIR_APP 	:= $(BASE_DIR)../sysroot/apps/bin/mkdir
SYSROOT_RENAME_APP 	:= $(BASE_DIR)../sysroot/apps/bin/rename
SYSROOT_CP_APP 		:= $(BASE_DIR)../sysroot/apps/bin/cp
SYSROOT_TOUCH_APP 	:= $(BASE_DIR)../sysroot/apps/bin/touch
SYSROOT_RM_APP 		:= $(BASE_DIR)../sysroot/apps/bin/rm
SYSROOT_CAT_APP 	:= $(BASE_DIR)../sysroot/apps/bin/cat
SYSROOT_ECHO_APP 	:= $(BASE_DIR)../sysroot/apps/bin/echo

LINKER_SCR  		:= $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding -O2
CFLAGS  += -I$(LIBC_INC)

LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

# FICHÈIROS FONTE (.c)
SH_SRC 		:= sh.c
LS_SRC 		:= ls.c
HELP_SRC 	:= help.c
MKDIR_SRC 	:= mkdir.c
RENAME_SRC 	:= rename.c
CP_SRC 		:= cp.c
TOUCH_SRC 	:= touch.c
RM_SRC 		:= rm.c
CAT_SRC 	:= cat.c
ECHO_SRC 	:= echo.c

# FICHÈIROS DE OBJETO (.o)
SH_OBJ 		:= sh.o
LS_OBJ 		:= ls.o
HELP_OBJ 	:= help.o
MKDIR_OBJ 	:= mkdir.o
RENAME_OBJ 	:= rename.o
CP_OBJ 		:= cp.o
TOUCH_OBJ 	:= touch.o
RM_OBJ 		:= rm.o
CAT_OBJ 	:= cat.o
ECHO_OBJ 	:= echo.o

CRT0    := $(CRT0_DIR)/crt0.o
LIBC_A  := $(LIBC_DIR)/libc.a

# REGRAS DE COMPILAÇÃO
.PHONY: all clean

all: $(SYSROOT_SH_APP) $(SYSROOT_LS_APP) $(SYSROOT_HELP_APP) \
$(SYSROOT_MKDIR_APP) $(SYSROOT_RENAME_APP) $(SYSROOT_CP_APP) \
$(SYSROOT_TOUCH_APP) $(SYSROOT_RM_APP) $(SYSROOT_CAT_APP) $(SYSROOT_ECHO_APP)

# LINKAGEM DOS EXECUTÁVEIS FINAIS
$(SYSROOT_SH_APP): $(SH_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_SH_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(SH_OBJ) $(LIBC_A) -o $@

$(SYSROOT_LS_APP): $(LS_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(LS_OBJ) $(LIBC_A) -o $@

$(SYSROOT_HELP_APP): $(HELP_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(HELP_OBJ) $(LIBC_A) -o $@

$(SYSROOT_MKDIR_APP): $(MKDIR_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(MKDIR_OBJ) $(LIBC_A) -o $@

$(SYSROOT_RENAME_APP): $(RENAME_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(RENAME_OBJ) $(LIBC_A) -o $@

$(SYSROOT_CP_APP): $(CP_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(CP_OBJ) $(LIBC_A) -o $@

$(SYSROOT_TOUCH_APP): $(TOUCH_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(TOUCH_OBJ) $(LIBC_A) -o $@

$(SYSROOT_RM_APP): $(RM_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(RM_OBJ) $(LIBC_A) -o $@

$(SYSROOT_CAT_APP): $(CAT_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(CAT_OBJ) $(LIBC_A) -o $@

$(SYSROOT_ECHO_APP): $(ECHO_OBJ) $(LIBC_A) $(CRT0)
	$(LD) $(LDFLAGS) $(CRT0) $(ECHO_OBJ) $(LIBC_A) -o $@

# COMPILAÇÃO DOS CÓDIGOS FONTE PARA OBJETO
$(SH_OBJ): $(SH_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(LS_OBJ): $(LS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(HELP_OBJ): $(HELP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(MKDIR_OBJ): $(MKDIR_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(RENAME_OBJ): $(RENAME_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(CP_OBJ): $(CP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOUCH_OBJ): $(TOUCH_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(RM_OBJ): $(RM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(CAT_OBJ): $(CAT_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(ECHO_OBJ): $(ECHO_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# LIMPEZA DO DIRETÓRIO
clean:
	@echo "[CLEAN] A remover ficheiros de objeto locais..."
	rm -f $(SH_OBJ) $(LS_OBJ) $(HELP_OBJ) $(MKDIR_OBJ) $(RENAME_OBJ) \
	$(CP_OBJ) $(TOUCH_OBJ) $(RM_OBJ) $(CAT_OBJ) $(ECHO_OBJ)