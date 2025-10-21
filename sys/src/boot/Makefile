CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
           -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
           -mno-red-zone -mcmodel=kernel -O2 -I. -I../libchar/include -nostdlib -fno-builtin -s
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

.PHONY: clean
clean:
	@echo "[*] Cleaning boot build..."
	@rm -rf obj kernel.elf
	@echo "[+] Boot build cleaned."

