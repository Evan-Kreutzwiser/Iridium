
CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld

CFLAGS := -I../public -I../libc/include -Wall -Wextra -Wpointer-arith -Wcast-align -Wredundant-decls -m64 -static -nostdlib -m64 -L../libc/ -lc -gdwarf-2
SRCS := init.c
TARGET := init.sys

.PHONY: clean

$(TARGET): $(SRCS) ../libc/libc.a Makefile
	$(CC) -o $@ $< $(CFLAGS)

clean:
	find . -type f -name '*.o' -delete
	-rm -f $(TARGET)
