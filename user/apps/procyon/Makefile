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

CC      := gcc
LD      := ld

BASE_DIR    := ../../
LIBC_DIR    := $(BASE_DIR)libc
LIBC_INC    := $(LIBC_DIR)/include
CRT0_DIR    := $(LIBC_DIR)/src/arch/x86_64
SYSROOT_APP := $(BASE_DIR)../sysroot/apps/procyon.elf
LINKER_SCR  := $(BASE_DIR)libc/user_x86_64.ld

CFLAGS  := -Wall -Wextra -m64 -O2 -nostdinc -ffreestanding -I$(LIBC_INC)
LDFLAGS := -m elf_x86_64 -nostdlib -T $(LINKER_SCR)

PROCYON_BASE_OBJ := procyon.o cpu_memory.o utf8_decode.o procyon_file.o window.o pixel.o \
                    text.o text_cache.o event.o image.o icon.o color.o draw_rect.o draw_line.o layer.o compositor.o \
                    framebuffer.o mouse_cursor.o procyon_ipc.o desktop.o

PROCYON_UI_OBJ   := components.o label.o button.o iconbutton.o checkbox.o listbox.o radiobutton.o \
					slider.o progressbar.o painel.o combobox.o editbox.o gridview.o

ALL_OBJS         := $(PROCYON_BASE_OBJ) $(PROCYON_UI_OBJ)
				
CRT0        := $(CRT0_DIR)/crt0.o
LIBC_A      := $(LIBC_DIR)/libc.a

.PHONY: all clean

all: $(SYSROOT_APP)

$(SYSROOT_APP): $(ALL_OBJS) $(LIBC_A) $(CRT0)
	@mkdir -p $(dir $(SYSROOT_APP))
	$(LD) $(LDFLAGS) $(CRT0) $(ALL_OBJS) $(LIBC_A) -o $@

$(PROCYON_BASE_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROCYON_UI_OBJ): %.o: ui/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJS)

