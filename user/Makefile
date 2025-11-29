CC := x86_64-elf-gcc
LD := x86_64-elf-ld
AR := ar

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
LIB_DIR := lib

LIBC_SRC_DIR := ../sys/src/libc
LIBC_INC_DIR := $(LIBC_SRC_DIR)/include

CRT0_SRC := crt0.S
CRT0 := $(LIB_DIR)/crt0.o
SYSCALL_SRC := syscall.c
SYSCALL_OBJ := $(OBJ_DIR)/syscall.o
LDS_SCRIPT := src/userland.lds
LIBUSER := $(LIB_DIR)/libuser.a

SYSINCDIR := ../sys/sys
SYS1INCDIR := ../sys
ARCHINCDIR := ../amd64/include

FEATURE_FLAGS := -D__POSIX_VISIBLE=200809 \
                 -D__XPG_VISIBLE=700 \
                 -D__ISO_C_VISIBLE=2011 \
                 -D__BSD_VISIBLE=1 \
                 -DNL_TEXTMAX=128

CFLAGS := $(FEATURE_FLAGS) -Wall -Wextra -std=gnu11 -ffreestanding -nostdlib -nostdinc \
          -fno-builtin -fno-stack-protector -O2 -ffunction-sections -fdata-sections \
          -Iinclude -I$(SYSINCDIR) -I$(SYS1INCDIR) -I$(LIBC_INC_DIR) -I$(ARCHINCDIR) \
          -Wno-builtin-macro-redefined

LDFLAGS := -nostdlib -z max-page-size=0x1000

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
PROGRAMS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SOURCES))

LIBC_STRING := $(wildcard $(LIBC_SRC_DIR)/string/*.c)
LIBC_GEN    := $(wildcard $(LIBC_SRC_DIR)/gen/*.c)
LIBC_SOURCES := $(LIBC_STRING) $(LIBC_GEN)
LIBC_OBJS := $(patsubst $(LIBC_SRC_DIR)/%.c,$(LIB_DIR)/%.o,$(LIBC_SOURCES))

.PHONY: all clean

all: $(LDS_SCRIPT) $(CRT0) $(SYSCALL_OBJ) $(LIBUSER) $(PROGRAMS)

$(OBJ_DIR) $(BIN_DIR) $(LIB_DIR):
	mkdir -p $@

$(LIB_DIR)/string $(LIB_DIR)/gen:
	mkdir -p $@

$(CRT0): $(CRT0_SRC) | $(LIB_DIR)
	$(CC) -c $(CRT0_SRC) -o $@
	@echo "[+] Built crt0.o"

$(SYSCALL_OBJ): $(SYSCALL_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SYSCALL_SRC) -o $@
	@echo "[+] Built syscall.o"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_DIR)/%.o: $(LIBC_SRC_DIR)/%.c | $(LIB_DIR) $(LIB_DIR)/string $(LIB_DIR)/gen
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBUSER): $(LIBC_OBJS) | $(LIB_DIR)
	$(AR) rcs $(LIBUSER) $(LIBC_OBJS)
	@echo "[+] Built libuser.a"

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(CRT0) $(SYSCALL_OBJ) $(LIBUSER) $(LDS_SCRIPT) | $(BIN_DIR)
	$(LD) -T $(LDS_SCRIPT) $(LDFLAGS) -o $@ $(CRT0) $< $(SYSCALL_OBJ) $(LIBUSER)
	@echo "[+] Built user program: $@"

$(LDS_SCRIPT):
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
	@echo "[+] Created userland linker script"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) $(LDS_SCRIPT)
	@echo "Cleaned userland build artifacts"