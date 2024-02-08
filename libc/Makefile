
CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld
AR ?= x86_64-elf-ar

CFLAGS		:= -O2 -g -m64 -fPIC
INCS		:= -I./include -I../public
WARNINGS	:= -Wall -Wextra -Wpointer-arith -Wcast-align -Wredundant-decls
LDFLAGS		:= -nostdlib -T ./$(ARCH_DIR)/linker.ld

SRCS	:= $(shell find . -type f -name '*.c' -o -type f -name '*.S')
OBJS	:= $(addsuffix .o, $(basename $(SRCS)))
DEPS	:= $(patsubst %.o, %.d, $(OBJS))

all: libc.a

libc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c %.d Makefile
	$(CC) $(CFLAGS) -MMD -MP $(INCS) $(WARNINGS) -c $< -o $@

%.o: %.S %.d Makefile
	$(CC) $(CFLAGS) -MMD -MP $(INCS) $(WARNINGS) -c $< -o $@

$(DEPS):

-include $(DEPS)

clean:
	find . -type f -name '*.o' -delete
	find . -type f -name '*.d' -delete
	-rm -f libc.a
