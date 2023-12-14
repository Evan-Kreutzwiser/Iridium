
CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld

CFLAGS := -I../public -Wall -Wextra -Wpointer-arith -Wcast-align -Wredundant-decls -m64 -static -ffreestanding -nostartfiles -gdwarf-2
SRCS := init.c
TARGET := init.sys

.PHONY: clean

$(TARGET): $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	find . -type f -name '*.o' -delete
	-rm -f $(TARGET)
