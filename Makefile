# ============================================================
# Sirius Education - Kernel
# ============================================================

CC := gcc
AS := nasm
LD := ld

KERNEL_DIR  := kernel/kernel
ARCH_DIR    := kernel/arch
LIB_DIR     := kernel/lib
DRIVERS_DIR := kernel/drivers
FS_DIR 		:= kernel/fs
BUILD_DIR   := build
LINKER      := scripts/linker/x86_64.ld
APLINKER    := scripts/linker/x86_64_ap.ld

TARGET := $(BUILD_DIR)/kernel.elf


# ============================================================
# Objetos Assembly
# ============================================================

ASM_OBJ := \
	$(BUILD_DIR)/entry.o \
	$(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/syscall_stub.o


# ============================================================
# Objetos C
# ============================================================

C_OBJ := \
	$(BUILD_DIR)/kernel_main.o \
	$(BUILD_DIR)/panic.o \
	$(BUILD_DIR)/boot_info.o \
	$(BUILD_DIR)/paging.o \
	$(BUILD_DIR)/paging_map_region_bitmap.o \
	$(BUILD_DIR)/vmm.o \
	$(BUILD_DIR)/vmm_scratch_window.o \
	$(BUILD_DIR)/cpu.o \
	$(BUILD_DIR)/idle.o \
	$(BUILD_DIR)/idt.o \
	$(BUILD_DIR)/isr.o \
	$(BUILD_DIR)/string.o \
	$(BUILD_DIR)/video.o \
	$(BUILD_DIR)/char.o \
	$(BUILD_DIR)/font.o \
	$(BUILD_DIR)/kprintf.o \
	$(BUILD_DIR)/pmm.o \
	$(BUILD_DIR)/heap.o \
	$(BUILD_DIR)/pool.o \
	$(BUILD_DIR)/acpi.o \
	$(BUILD_DIR)/lapic.o \
	$(BUILD_DIR)/ioapic.o \
	$(BUILD_DIR)/irq.o \
	$(BUILD_DIR)/msi.o \
	$(BUILD_DIR)/smp.o \
	$(BUILD_DIR)/scheduler.o \
	$(BUILD_DIR)/thread.o \
	$(BUILD_DIR)/process.o \
	$(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/pci.o \
	$(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/mouse.o \
	$(BUILD_DIR)/ahci.o \
	$(BUILD_DIR)/block.o \
	$(BUILD_DIR)/vfs.o \
	$(BUILD_DIR)/test.o



# ============================================================
# Compiler flags
# ============================================================

CFLAGS := -m64 \
          -ffreestanding \
          -fno-pie \
          -fno-stack-protector \
          -fno-omit-frame-pointer \
          -mno-red-zone \
          -mcmodel=kernel \
          -nostdlib \
          -nostdinc \
          -Wall \
          -Wextra \
          -I./include

ASFLAGS := -f elf64

LDFLAGS := -m elf_x86_64 \
           -Map kernel.map -T $(LINKER)


# ============================================================
# Default target
# ============================================================

.PHONY: all clean

all: $(TARGET) $(BUILD_DIR)/trampoline.bin


# ============================================================
# Criar pasta build
# ============================================================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# ============================================================
# Assemble trampoline.asm
#
# O trampoline é código binário puro:
# NÃO é objeto ELF.
# ============================================================

$(BUILD_DIR)/trampoline.bin: $(ARCH_DIR)/x86_64/boot/trampoline.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@

# ============================================================
# Assemble entry.asm
# ============================================================

$(BUILD_DIR)/entry.o: $(ARCH_DIR)/x86_64/boot/entry.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# ============================================================
# Assemble interrupt.asm
# ============================================================

$(BUILD_DIR)/interrupt.o: $(ARCH_DIR)/x86_64/cpu/interrupt.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# ============================================================
# Assemble syscall_stub.asm
# ============================================================

$(BUILD_DIR)/syscall_stub.o: $(ARCH_DIR)/x86_64/cpu/syscall_stub.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@



# ============================================================
# Compile kernel_main.c
# ============================================================

$(BUILD_DIR)/kernel_main.o: $(KERNEL_DIR)/core/kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile panic.c
# ============================================================

$(BUILD_DIR)/panic.o: $(KERNEL_DIR)/core/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Compile boot_info.c
# ============================================================

$(BUILD_DIR)/boot_info.o: $(KERNEL_DIR)/core/boot_info.c | $(BUILD_DIR)
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
# Compile vmm.c
# ============================================================

$(BUILD_DIR)/vmm.o: $(ARCH_DIR)/x86_64/mm/vmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Compile vmm_scratch_window.c
# ============================================================

$(BUILD_DIR)/vmm_scratch_window.o: $(ARCH_DIR)/x86_64/mm/vmm_scratch_window.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Compile cpu.c
# ============================================================

$(BUILD_DIR)/cpu.o: $(ARCH_DIR)/x86_64/cpu/cpu.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile idle.c
# ============================================================

$(BUILD_DIR)/idle.o: $(ARCH_DIR)/x86_64/cpu/idle.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Compile idt.c
# ============================================================

$(BUILD_DIR)/idt.o: $(ARCH_DIR)/x86_64/cpu/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Compile isr.c
# ============================================================

$(BUILD_DIR)/isr.o: $(ARCH_DIR)/x86_64/cpu/isr.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile irq.c
# ============================================================

$(BUILD_DIR)/irq.o: $(ARCH_DIR)/x86_64/kapi/irq.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile msi.c
# ============================================================

$(BUILD_DIR)/msi.o: $(ARCH_DIR)/x86_64/kapi/msi.c | $(BUILD_DIR)
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
# Compile pci.c
# ============================================================

$(BUILD_DIR)/pci.o: $(DRIVERS_DIR)/bus/pci.c | $(BUILD_DIR)
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
# Compile keyboard.c
# ============================================================

$(BUILD_DIR)/keyboard.o: $(DRIVERS_DIR)/char/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile mouse.c
# ============================================================

$(BUILD_DIR)/mouse.o: $(DRIVERS_DIR)/char/mouse.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile ahci.c
# ============================================================

$(BUILD_DIR)/ahci.o: $(DRIVERS_DIR)/storage/ahci.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile block.c
# ============================================================

$(BUILD_DIR)/block.o: $(DRIVERS_DIR)/storage/block.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile test.c
# ============================================================

$(BUILD_DIR)/test.o: $(KERNEL_DIR)/core/test.c | $(BUILD_DIR)
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
# Compile heap.c
# ============================================================

$(BUILD_DIR)/heap.o: $(KERNEL_DIR)/mm/heap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile pool.c
# ============================================================

$(BUILD_DIR)/pool.o: $(KERNEL_DIR)/mm/pool.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile acpi.c
# ============================================================

$(BUILD_DIR)/acpi.o: $(DRIVERS_DIR)/bus/acpi.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile lapic.c
# ============================================================

$(BUILD_DIR)/lapic.o: $(ARCH_DIR)/x86_64/cpu/lapic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile ioapic.c
# ============================================================

$(BUILD_DIR)/ioapic.o: $(ARCH_DIR)/x86_64/cpu/ioapic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile smp.c
# ============================================================

$(BUILD_DIR)/smp.o: $(ARCH_DIR)/x86_64/cpu/smp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile scheduler.c
# ============================================================

$(BUILD_DIR)/scheduler.o: $(KERNEL_DIR)/sched/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile thread.c
# ============================================================

$(BUILD_DIR)/thread.o: $(KERNEL_DIR)/sched/thread.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile process.c
# ============================================================

$(BUILD_DIR)/process.o: $(KERNEL_DIR)/sched/process.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile syscall.c
# ============================================================

$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Compile vfs.c
# ============================================================

$(BUILD_DIR)/vfs.o: $(FS_DIR)/vfs/vfs.c | $(BUILD_DIR)
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