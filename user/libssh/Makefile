CC      := gcc
ASM     := nasm
AR      := ar
RANLIB  := ranlib

CFLAGS  := -Wall -Wextra -m64 -nostdinc -ffreestanding -I./include
ASMFLAGS:= -f elf64

TARGET  := libssh.a

C_SRCS   := libssh.c tweetnacl.c extra.c
OBJS     := $(C_SRCS:.c=.o)

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(TARGET)

re: fclean all