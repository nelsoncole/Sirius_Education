CC       := gcc
AR       := ar
RANLIB   := ranlib

LIBC_INC := ../libc/include
CFLAGS   := -Wall -Wextra -m64 -O2 -nostdinc -ffreestanding -I./include -I$(LIBC_INC)

TARGET   := procyon.a

PROCYON_BASE_OBJ := cpu_memory.o utf8_decode.o procyon_file.o window.o pixel.o \
                    text.o text_cache.o event.o image.o icon.o color.o draw_rect.o \
					draw_line.o layer.o compositor.o framebuffer.o mouse_cursor.o \
					procyon_ipc.o

PROCYON_UI_OBJ   := components.o label.o button.o iconbutton.o checkbox.o listbox.o radiobutton.o \
                    slider.o progressbar.o painel.o combobox.o editbox.o gridview.o

OBJS     := $(PROCYON_BASE_OBJ) $(PROCYON_UI_OBJ)

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

$(PROCYON_BASE_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROCYON_UI_OBJ): %.o: ui/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(TARGET)

re: fclean all
