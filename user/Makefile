CC := x86_64-elf-gcc
LD := x86_64-elf-ld
AR := ar

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
LIB_DIR := lib

CRT0_SRC := crt0.S
CRT0 := $(LIB_DIR)/crt0.o
LIBUSER := $(LIB_DIR)/libuser.a
SYSCALL_SRC := syscall.c
SYSCALL_OBJ := $(OBJ_DIR)/syscall.o

LDS_SCRIPT := src/userland.lds

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -nostdlib -nostdinc \
           -fno-builtin -fno-stack-protector -O2 -ffunction-sections -fdata-sections \
           -Iinclude -I../sys/sys -I../sys/src/libc/include -I../amd64/include

LDFLAGS := -nostdlib -z max-page-size=0x1000

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
PROGRAMS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SOURCES))

.PHONY: all clean

all: $(LDS_SCRIPT) $(CRT0) $(SYSCALL_OBJ) $(PROGRAMS)

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

$(LIB_DIR):
	mkdir -p $@

$(CRT0): $(CRT0_SRC) | $(LIB_DIR)
	$(CC) -c $(CRT0_SRC) -o $@
	@echo "[+] Built crt0.o"

$(SYSCALL_OBJ): $(SYSCALL_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SYSCALL_SRC) -o $@
	@echo "[+] Built syscall.o"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LDS_SCRIPT):
	@echo "Creating linker script..."
	@echo "OUTPUT_FORMAT(elf64-x86-64)" > $@
	@echo "ENTRY(_start)" >> $@
	@echo "SECTIONS {" >> $@
	@echo "  . = 0x400000;" >> $@
	@echo "  .text : { *(.text .text.*) }" >> $@
	@echo "  . = ALIGN(0x1000);" >> $@
	@echo "  .rodata : { *(.rodata .rodata.*) }" >> $@
	@echo "  . = ALIGN(0x1000);" >> $@
	@echo "  .data : { *(.data .data.*) }" >> $@
	@echo "  .bss : { *(.bss .bss.*) *(COMMON) }" >> $@
	@echo "  /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment) }" >> $@
	@echo "}" >> $@
	@echo "[+] Created linker script: $@"

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(CRT0) $(SYSCALL_OBJ) $(LDS_SCRIPT) | $(BIN_DIR)
	$(LD) -T $(LDS_SCRIPT) $(LDFLAGS) -o $@ $(CRT0) $< $(SYSCALL_OBJ)
	@echo "[+] Built user program: $@"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) $(LDS_SCRIPT)
	@echo "Cleaned userland build artifacts"