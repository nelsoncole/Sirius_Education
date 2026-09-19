# ============================================================
# Sirius Education - Kernel
# ============================================================

CC := gcc
AS := nasm
LD := ld

# ============================================================
# Diretórios do projeto
# ============================================================

KERNEL_DIR  := kernel/kernel
ARCH_DIR    := kernel/arch
LIB_DIR     := kernel/lib
DRIVERS_DIR := kernel/drivers
FS_DIR      := kernel/fs
KMODS_DIR   := kernel/kmods

USER_DIR        := user
USER_BUILD_DIR  := build/user

BUILD_DIR       := build
SYSROOT_DIR     := sysroot
SYSROOT_SYSTEM  := $(SYSROOT_DIR)/System

LINKER      := scripts/linker/x86_64.ld
USER_LINKER := $(USER_DIR)/lib/user_x86_64.ld

# ============================================================
# Artefactos de BUILD
#
# IMPORTANTE:
# Estes ficheiros NÃO são gravados em sysroot/System.
# O sysroot só será utilizado numa etapa posterior de instalação.
# ============================================================

KERNEL_TARGET     := $(BUILD_DIR)/kernel.elf
TRAMPOLINE_TARGET  := $(BUILD_DIR)/trampoline.bin
USER_TARGET        := $(USER_BUILD_DIR)/user.elf

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
	$(BUILD_DIR)/spinlock.o \
	$(BUILD_DIR)/paging.o \
	$(BUILD_DIR)/paging_map_region_bitmap.o \
	$(BUILD_DIR)/vmm.o \
	$(BUILD_DIR)/vmm_scratch_window.o \
	$(BUILD_DIR)/cpu.o \
	$(BUILD_DIR)/idle.o \
	$(BUILD_DIR)/idt.o \
	$(BUILD_DIR)/isr.o \
	$(BUILD_DIR)/fault.o \
	$(BUILD_DIR)/string.o \
	$(BUILD_DIR)/sse_memcpy.o \
	$(BUILD_DIR)/sse_memset.o \
	$(BUILD_DIR)/video.o \
	$(BUILD_DIR)/char.o \
	$(BUILD_DIR)/font.o \
	$(BUILD_DIR)/kprintf.o \
	$(BUILD_DIR)/pmm.o \
	$(BUILD_DIR)/heap.o \
	$(BUILD_DIR)/pool.o \
	$(BUILD_DIR)/brk.o \
	$(BUILD_DIR)/acpi.o \
	$(BUILD_DIR)/timer.o \
	$(BUILD_DIR)/lapic.o \
	$(BUILD_DIR)/ioapic.o \
	$(BUILD_DIR)/irq.o \
	$(BUILD_DIR)/msi.o \
	$(BUILD_DIR)/smp.o \
	$(BUILD_DIR)/scheduler.o \
	$(BUILD_DIR)/thread.o \
	$(BUILD_DIR)/process.o \
	$(BUILD_DIR)/process_loader.o \
	$(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/pci.o \
	$(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/mouse.o \
	$(BUILD_DIR)/ahci.o \
	$(BUILD_DIR)/block.o \
	$(BUILD_DIR)/partitions.o \
	$(BUILD_DIR)/tty.o \
	$(BUILD_DIR)/vfs.o \
	$(BUILD_DIR)/ramfs.o \
	$(BUILD_DIR)/vfs_tty.o \
	$(BUILD_DIR)/console.o \
	$(BUILD_DIR)/fat32.o \
	$(BUILD_DIR)/socket.o \
	$(BUILD_DIR)/af_local.o \
	$(BUILD_DIR)/af_inet.o \
	$(BUILD_DIR)/pf_packet.o \
	$(BUILD_DIR)/net.o \
	$(BUILD_DIR)/ip.o \
	$(BUILD_DIR)/udp.o \
	$(BUILD_DIR)/tcp.o \
	$(BUILD_DIR)/arp.o \
	$(BUILD_DIR)/dhcp.o \
	$(BUILD_DIR)/kmod.o \
	$(BUILD_DIR)/symbols.o \
	$(BUILD_DIR)/loader.o \
	$(BUILD_DIR)/kmod_loader.o \
	$(BUILD_DIR)/test.o

# ============================================================
# Objetos do User Space
# ============================================================

USER_OBJS := \
	$(USER_BUILD_DIR)/crt0.o \
	$(USER_BUILD_DIR)/user.o

# ============================================================
# Compiler Flags - Kernel
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

# ============================================================
# Compiler Flags - User Space / Ring 3
# ============================================================

USER_CFLAGS := -m64 \
               -ffreestanding \
               -fno-pie \
               -fno-stack-protector \
               -fno-omit-frame-pointer \
               -mno-red-zone \
               -nostdlib \
               -nostdinc \
               -Wall \
               -Wextra \
               -I./include

# ============================================================
# Assembly / Linker Flags
# ============================================================

ASFLAGS := -f elf64

LDFLAGS := -m elf_x86_64 \
           -Map $(BUILD_DIR)/kernel.map \
           -T $(LINKER)

USER_LDFLAGS := -m elf_x86_64 \
                -T $(USER_LINKER)

# ============================================================
# Targets principais
# ============================================================

.PHONY: all kernel user_space trampoline clean install

all: kernel user_space trampoline

kernel: $(KERNEL_TARGET)

user_space: $(USER_TARGET)

trampoline: $(TRAMPOLINE_TARGET)

# ============================================================
# Criar diretórios
# ============================================================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(USER_BUILD_DIR): | $(BUILD_DIR)
	mkdir -p $(USER_BUILD_DIR)

# ============================================================
# User Space
# ============================================================

$(USER_TARGET): $(USER_OBJS) $(USER_LINKER) | $(USER_BUILD_DIR)
	$(LD) $(USER_LDFLAGS) $(USER_OBJS) -o $@

$(USER_BUILD_DIR)/crt0.o: $(USER_DIR)/lib/crt0.asm | $(USER_BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(USER_BUILD_DIR)/user.o: $(USER_DIR)/user.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

# ============================================================
# Trampoline
#
# Binário RAW, não ELF.
# ============================================================

$(TRAMPOLINE_TARGET): $(ARCH_DIR)/x86_64/boot/trampoline.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@

# ============================================================
# Assembly do Kernel
# ============================================================

$(BUILD_DIR)/entry.o: $(ARCH_DIR)/x86_64/boot/entry.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: $(ARCH_DIR)/x86_64/cpu/interrupt.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/syscall_stub.o: $(ARCH_DIR)/x86_64/cpu/syscall_stub.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# ============================================================
# Kernel C
# ============================================================

$(BUILD_DIR)/kernel_main.o: $(KERNEL_DIR)/core/kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: $(KERNEL_DIR)/core/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot_info.o: $(KERNEL_DIR)/core/boot_info.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/spinlock.o: $(KERNEL_DIR)/core/spinlock.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/paging.o: $(ARCH_DIR)/x86_64/mm/paging.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/paging_map_region_bitmap.o: $(ARCH_DIR)/x86_64/mm/paging_map_region_bitmap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm.o: $(ARCH_DIR)/x86_64/mm/vmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm_scratch_window.o: $(ARCH_DIR)/x86_64/mm/vmm_scratch_window.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cpu.o: $(ARCH_DIR)/x86_64/cpu/cpu.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idle.o: $(ARCH_DIR)/x86_64/cpu/idle.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: $(ARCH_DIR)/x86_64/cpu/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/isr.o: $(ARCH_DIR)/x86_64/cpu/isr.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fault.o: $(ARCH_DIR)/x86_64/cpu/fault.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/irq.o: $(ARCH_DIR)/x86_64/kapi/irq.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/msi.o: $(ARCH_DIR)/x86_64/kapi/msi.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Biblioteca
# ============================================================

$(BUILD_DIR)/string.o: $(LIB_DIR)/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sse_memcpy.o: $(LIB_DIR)/sse_memcpy.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sse_memset.o: $(LIB_DIR)/sse_memset.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kprintf.o: $(LIB_DIR)/kprintf.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Drivers
# ============================================================

$(BUILD_DIR)/video.o: $(DRIVERS_DIR)/video/video.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pci.o: $(DRIVERS_DIR)/bus/pci.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/char.o: $(DRIVERS_DIR)/char/char.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/font.o: $(DRIVERS_DIR)/char/font.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(DRIVERS_DIR)/char/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mouse.o: $(DRIVERS_DIR)/char/mouse.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ahci.o: $(DRIVERS_DIR)/storage/ahci.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/block.o: $(DRIVERS_DIR)/storage/block.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/partitions.o: $(DRIVERS_DIR)/storage/partitions.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tty.o: $(DRIVERS_DIR)/tty/tty.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/console.o: $(DRIVERS_DIR)/video/console.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Kernel
# ============================================================

$(BUILD_DIR)/test.o: $(KERNEL_DIR)/core/test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: $(KERNEL_DIR)/mm/pmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/heap.o: $(KERNEL_DIR)/mm/heap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pool.o: $(KERNEL_DIR)/mm/pool.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/brk.o: $(KERNEL_DIR)/mm/brk.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/acpi.o: $(ARCH_DIR)/x86_64/kapi/acpi.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: $(ARCH_DIR)/x86_64/kapi/timer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lapic.o: $(ARCH_DIR)/x86_64/cpu/lapic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ioapic.o: $(ARCH_DIR)/x86_64/cpu/ioapic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/smp.o: $(ARCH_DIR)/x86_64/cpu/smp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/scheduler.o: $(KERNEL_DIR)/sched/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/thread.o: $(KERNEL_DIR)/sched/thread.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process.o: $(KERNEL_DIR)/sched/process.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process_loader.o: $(KERNEL_DIR)/sched/process_loader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: $(FS_DIR)/vfs/vfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ramfs.o: $(FS_DIR)/ramfs/ramfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs_tty.o: $(FS_DIR)/dev/vfs_tty.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fat32.o: $(FS_DIR)/fat/fat32.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/socket.o: $(KERNEL_DIR)/net/socket.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/af_local.o: $(KERNEL_DIR)/net/af_local.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/af_inet.o: $(KERNEL_DIR)/net/af_inet.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pf_packet.o: $(KERNEL_DIR)/net/pf_packet.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/net.o: $(KERNEL_DIR)/net/net.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ip.o: $(KERNEL_DIR)/net/ip.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/udp.o: $(KERNEL_DIR)/net/udp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tcp.o: $(KERNEL_DIR)/net/tcp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arp.o: $(KERNEL_DIR)/net/arp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dhcp.o: $(KERNEL_DIR)/net/dhcp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kmod.o: $(KMODS_DIR)/manager/kmod.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/symbols.o: $(KMODS_DIR)/manager/symbols.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loader.o: $(KMODS_DIR)/loader/loader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kmod_loader.o: $(KMODS_DIR)/loader/kmod_loader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Link Kernel
# ============================================================

$(KERNEL_TARGET): $(ASM_OBJ) $(C_OBJ) $(LINKER) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(ASM_OBJ) $(C_OBJ) -o $@

# ============================================================
# Instalação no Sysroot
#
# ATENÇÃO:
# O build NÃO coloca automaticamente os artefactos no sysroot.
#
# Apenas "make install" faz a cópia.
# ============================================================

install: all
	mkdir -p $(SYSROOT_SYSTEM)
	cp $(KERNEL_TARGET)    $(SYSROOT_SYSTEM)/kernel.elf
	cp $(TRAMPOLINE_TARGET) $(SYSROOT_SYSTEM)/trampoline.bin
	cp $(USER_TARGET)      $(SYSROOT_SYSTEM)/user.elf

# ============================================================
# Limpeza
# ============================================================

clean:
	rm -rf $(BUILD_DIR)