CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld

INCLUDES := -I. -I../libc/include -I../libchar/include -I../libkb/include \
            -I../libmem/include -I../libprintf/include -I../../../amd64/include \
			-I../../../sys/sys -I../../../sys

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 \
          $(INCLUDES) \
          -nostdlib -fno-builtin -s -DPRINTF_INCLUDE_CONFIG_H

LDFLAGS := -nostdlib -static -T linker.lds -z max-page-size=0x1000

INIT_C_SRC := $(wildcard init/*.c)
CPU_C_SRC := $(wildcard ../../../amd64/cpu/*.c)
CPU_ASM_SRC := $(wildcard ../../../amd64/cpu/*.S)

INIT_OBJS := $(patsubst init/%.c,obj/init/%.o,$(INIT_C_SRC))
CPU_C_OBJS := $(patsubst ../../../amd64/cpu/%.c,obj/cpu/%_c.o,$(CPU_C_SRC))
CPU_ASM_OBJS := $(patsubst ../../../amd64/cpu/%.S,obj/cpu/%_asm.o,$(CPU_ASM_SRC))
OBJS := $(INIT_OBJS) $(CPU_C_OBJS) $(CPU_ASM_OBJS)

LIBCHAR := ../libchar/lib/libchar.a
LIBC := ../libc/lib/libc.a 
LIBKB := ../libkb/lib/libkb.a
LIBMEM := ../libmem/lib/libmem.a
LIBPRINTF := ../libprintf/lib/libprintf.a

TARGET := uesi.elf

all: $(TARGET)

obj/init/%.o: init/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/cpu/%_c.o: ../../../amd64/cpu/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/cpu/%_asm.o: ../../../amd64/cpu/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBCHAR) $(LIBC) $(LIBKB) $(LIBMEM) $(LIBPRINTF)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBCHAR) $(LIBC) $(LIBKB) $(LIBMEM) $(LIBPRINTF) $(LIBC)
	@echo "Kernel built: $(TARGET)"
	@echo "Objects compiled:"
	@echo "  Init objects: $(INIT_OBJS)"
	@echo "  CPU C objects: $(CPU_C_OBJS)"
	@echo "  CPU ASM objects: $(CPU_ASM_OBJS)"

.PHONY: all clean

clean:
	@echo "[*] Cleaning boot build..."
	@rm -rf obj uesi.elf
	@echo "[+] Boot build cleaned."