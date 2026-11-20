CC      := gcc
LD      := ld

BASE_DIR    := ../../
LIBC_DIR    := $(BASE_DIR)libc
LIBC_INC    := $(LIBC_DIR)/include
PROCYON_DIR := $(BASE_DIR)procyon
PROCYON_INC := $(BASE_DIR)
CRT0_DIR    := $(LIBC_DIR)/src/arch/x86_64
SYSROOT_APP := $(BASE_DIR)../sysroot/usr/app/editor
LINKER_SCR  := $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -O2 -nostdinc -ffreestanding
CFLAGS  += -I$(LIBC_INC) -I$(PROCYON_INC)

LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

CRT0    := $(CRT0_DIR)/crt0.o
EDITOR_OBJ := editor.o

LIBC_A   := $(LIBC_DIR)/libc.a
PROCYON_A := $(PROCYON_DIR)/procyon.a

.PHONY: all clean

all: $(SYSROOT_APP)

$(SYSROOT_APP): $(EDITOR_OBJ) $(PROCYON_A) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(EDITOR_OBJ) $(PROCYON_A) $(LIBC_A) -o $@

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EDITOR_OBJ)
