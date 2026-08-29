# ============================================================
# Sirius Education - Kernel
# ============================================================

CC := gcc
AS := nasm
LD := ld

KERNEL_DIR := src/kernel
ARCH_DIR   := src/arch
LIB_DIR    := src/lib
DRIVERS_DIR    := src/drivers
BUILD_DIR  := build
LINKER     := scripts/linker/x86_64.ld

TARGET := $(BUILD_DIR)/kernel.elf

ASM_SRC := $(ARCH_DIR)/x86_64/boot/entry.asm

C_SRC   := \
	$(KERNEL_DIR)/core/kernel_main.c \
	$(ARCH_DIR)/x86_64/mm/paging.c \
	$(ARCH_DIR)/x86_64/mm/paging_map_region_bitmap.c \
	$(ARCH_DIR)x86_64/cpu/cpu.c \
	$(LIB_DIR)/string.c \
    $(DRIVERS_DIR)/video/video.c \
	$(DRIVERS_DIR)/char/char.c \
	$(DRIVERS_DIR)/char/font.c \
	$(LIB_DIR)/kprintf.c \
	$(KERNEL_DIR)/mm/pmm.c \

ASM_OBJ := $(BUILD_DIR)/entry.o

C_OBJ   := \
	$(BUILD_DIR)/kernel_main.o \
	$(BUILD_DIR)/paging.o \
	$(BUILD_DIR)/paging_map_region_bitmap.o \
	$(BUILD_DIR)/cpu.o \
	$(BUILD_DIR)/string.o \
    $(BUILD_DIR)/video.o \
	$(BUILD_DIR)/char.o \
	$(BUILD_DIR)/font.o \
	$(BUILD_DIR)/kprintf.o \
	$(BUILD_DIR)/pmm.o


# ============================================================
# Compiler flags
# ============================================================

CFLAGS := -m64 \
          -ffreestanding \
          -fno-pie \
          -fno-stack-protector \
          -mno-red-zone \
		  -mcmodel=kernel \
          -Wall \
          -Wextra \
          -I./include

ASFLAGS := -f elf64

LDFLAGS := -m elf_x86_64 \
           -T $(LINKER)


# ============================================================
# Default target
# ============================================================

.PHONY: all clean

all: $(TARGET)


# ============================================================
# Criar pasta build
# ============================================================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# ============================================================
# Assemble entry.asm
# ============================================================

$(BUILD_DIR)/entry.o: $(ASM_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# ============================================================
# Compile kernel_main.c
# ============================================================

$(BUILD_DIR)/kernel_main.o: $(KERNEL_DIR)/core/kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile paging.c
# ============================================================

$(BUILD_DIR)/paging.o: $(ARCH_DIR)/x86_64/mm/paging.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile paging_map_region_bitmap.c
# ============================================================

$(BUILD_DIR)/paging_map_region_bitmap.o: $(ARCH_DIR)/x86_64/mm/paging_map_region_bitmap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile cpu.c
# ============================================================

$(BUILD_DIR)/cpu.o: $(ARCH_DIR)/x86_64/cpu/cpu.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile string.c
# ============================================================

$(BUILD_DIR)/string.o: $(LIB_DIR)/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile video.c
# ============================================================

$(BUILD_DIR)/video.o: $(DRIVERS_DIR)/video/video.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile char.c
# ============================================================

$(BUILD_DIR)/char.o: $(DRIVERS_DIR)/char/char.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile font.c
# ============================================================

$(BUILD_DIR)/font.o: $(DRIVERS_DIR)/char/font.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile kprintf.c
# ============================================================

$(BUILD_DIR)/kprintf.o: $(LIB_DIR)/kprintf.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile pmm.c
# ============================================================

$(BUILD_DIR)/pmm.o: $(KERNEL_DIR)/mm/pmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Link kernel
# ============================================================

$(TARGET): $(ASM_OBJ) $(C_OBJ) $(LINKER)
	$(LD) $(LDFLAGS) $(ASM_OBJ) $(C_OBJ) -o $@


# ============================================================
# Clean
# ============================================================

clean:
	rm -rf $(BUILD_DIR)/*