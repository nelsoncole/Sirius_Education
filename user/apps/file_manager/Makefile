CC      := gcc
LD      := ld

BASE_DIR    := ../../
LIBC_DIR    := $(BASE_DIR)libc
LIBC_INC    := $(LIBC_DIR)/include
PROCYON_DIR := $(BASE_DIR)procyon
PROCYON_INC := $(BASE_DIR)
CRT0_DIR    := $(LIBC_DIR)/src/arch/x86_64
SYSROOT_APP := $(BASE_DIR)../sysroot/usr/app
LINKER_SCR  := $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -O2 -nostdinc -ffreestanding
CFLAGS  += -I$(LIBC_INC) -I$(PROCYON_INC)

LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

CRT0    := $(CRT0_DIR)/crt0.o

COMMON_OBJ := file_manager.o

APP_OBJ  := file_manager_app.o
OPEN_OBJ := file_manager_open.o
SAVE_OBJ := file_manager_save.o

APP_BIN  := $(SYSROOT_APP)/file_manager
OPEN_BIN := $(SYSROOT_APP)/file_manager_open
SAVE_BIN := $(SYSROOT_APP)/file_manager_save

LIBC_A   := $(LIBC_DIR)/libc.a
PROCYON_A := $(PROCYON_DIR)/procyon.a

.PHONY: all clean

all: $(APP_BIN) $(OPEN_BIN) $(SAVE_BIN)

$(APP_BIN): $(COMMON_OBJ) $(APP_OBJ) $(PROCYON_A) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(APP_BIN))
	$(LD) $(LDFLAGS) $(CRT0) $(COMMON_OBJ) $(APP_OBJ) $(PROCYON_A) $(LIBC_A) -o $@

$(OPEN_BIN): $(COMMON_OBJ) $(OPEN_OBJ) $(PROCYON_A) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(OPEN_BIN))
	$(LD) $(LDFLAGS) $(CRT0) $(COMMON_OBJ) $(OPEN_OBJ) $(PROCYON_A) $(LIBC_A) -o $@

$(SAVE_BIN): $(COMMON_OBJ) $(SAVE_OBJ) $(PROCYON_A) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SAVE_BIN))
	$(LD) $(LDFLAGS) $(CRT0) $(COMMON_OBJ) $(SAVE_OBJ) $(PROCYON_A) $(LIBC_A) -o $@

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(COMMON_OBJ) $(APP_OBJ) $(OPEN_OBJ) $(SAVE_OBJ)
	rm -f $(APP_BIN) $(OPEN_BIN) $(SAVE_BIN)