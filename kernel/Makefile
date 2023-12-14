# Disable implicit rules.
# Fixes an issue where make incorrectly uses the
# implicit rule instead of using the one defined here
#.SUFFIXES:

ARCH ?= x86_64

CC := x86_64-elf-gcc
LD := x86_64-elf-ld

ARCH_DIR := arch/$(ARCH)

# -fsanitize=undefined
CFLAGS := -ffreestanding -fno-pic -fno-pie -mgeneral-regs-only -mno-red-zone -z max-page-size=0x1000 -mcmodel=kernel -m64 -gdwarf-2
INCS := -I./arch/$(ARCH)/include -I./include -I../public
WARNINGS := -Wall -Wextra -Wpointer-arith -Wcast-align -Wredundant-decls
LDFLAGS := -nostdlib -T ./$(ARCH_DIR)/linker.ld

ARCH_SRCS := $(shell find ./$(ARCH_DIR) -type f -name '*.c' -o -type f -name '*.S' )
ARCH_OBJS := $(addsuffix .o, $(basename $(ARCH_SRCS)))
ARCH_DEPS := $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(ARCH_SRCS)))

TARGET := kernel.sys

SRCS     := $(shell find . -type f -name '*.c' -not -path '*/arch/*')
OBJS     := $(patsubst %.c, %.o, $(SRCS))
DEPS     := $(patsubst %.c, %.d, $(SRCS))

.PHONY: clean test

all: $(TARGET)

$(TARGET): $(OBJS) $(ARCH_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(ARCH_OBJS) -o $@

%.o: %.c %.d makefile
	$(CC) $(CFLAGS) -MMD -MP $(INCS) $(WARNINGS) -c $< -o $@

%.o: %.S %.d makefile
	$(CC) $(CFLAGS) -MMD -MP $(INCS) $(WARNINGS) -c $< -o $@

$(DEPS) $(ARCH_DEPS):

include $(DEPS)
include $(ARCH_DEPS)

clean:
	find . -type f -name '*.o' -delete
	find . -type f -name '*.d' -delete
	-rm -f $(TARGET)
