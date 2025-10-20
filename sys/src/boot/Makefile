CC := gcc
LD := ld

CFLAGS := -Wall -Wextra -std=c11 -ffreestanding -nostdlib \
           -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -O2 \
           -I. -I../libchar/include
LDFLAGS := -nostdlib -static -T linker.lds -z max-page-size=0x1000

SRC := $(wildcard init/*.c)
OBJS := $(patsubst %.c,obj/%.o,$(SRC))
LIBCHAR := ../libchar/lib/libchar.a
TARGET := kernel.elf

all: $(TARGET)

obj/%.o: %.c
	mkdir -p $(dir $@)
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBCHAR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBCHAR)
	@echo "Kernel built: $(TARGET)"

.PHONY: all
