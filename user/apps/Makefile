CC      := gcc
LD      := ld

BASE_DIR    	:= ../
LIBC_DIR        := $(BASE_DIR)libc
LIBC_INC        := $(LIBC_DIR)/include
CRT0_DIR        := $(LIBC_DIR)/src/arch/x86_64
SYSROOT_USER_APP 	:= $(BASE_DIR)../sysroot/apps/user.elf
SYSROOT_TEST_APP 	:= $(BASE_DIR)../sysroot/apps/test.elf
LINKER_SCR  	:= $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding -O2
CFLAGS  += -I$(LIBC_INC)

LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

USER_SRC := user.c
TEST_SRC := test.c

USER_OBJ := user.o
TEST_OBJ := test.o

CRT0    := $(CRT0_DIR)/crt0.o
LIBC_A  := $(LIBC_DIR)/libc.a

# REGRAS DE COMPILAÇÃO
.PHONY: all clean

all: $(SYSROOT_USER_APP) $(SYSROOT_TEST_APP)

$(SYSROOT_USER_APP): $(USER_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_USER_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(USER_OBJ) $(LIBC_A) -o $@

$(SYSROOT_TEST_APP): $(TEST_OBJ) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_TEST_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(TEST_OBJ) $(LIBC_A) -o $@

$(USER_OBJ): $(USER_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	@echo "[CLEAN] A remover ficheiros de objeto locais..."
	rm -f $(USER_OBJ) $(TEST_OBJ)
