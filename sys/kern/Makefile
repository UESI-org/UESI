CC ?= x86_64-elf-gcc
AS ?= x86_64-elf-as
AR := ar
INCDIR := ../sys
LIBKERNDIR := libkern
ARCHDIR := $(LIBKERNDIR)/arch/amd64
OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libkern.a

INCLUDES := -I.. -I../.. -I../../amd64/include -I../sys -I../src/libc/include -I../src/libmem/include -I../src/libprintf/include -I../src/libuserland/include -I../src/boot/include -I$(LIBKERNDIR) -nostdinc

FEATURE_FLAGS := -D__POSIX_VISIBLE=200809 \
                 -D__XPG_VISIBLE=700 \
                 -D__ISO_C_VISIBLE=2011 \
                 -D_BSD_SOURCE \
                 -DNL_TEXTMAX=128

CFLAGS := $(FEATURE_FLAGS) -D_KERNEL -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 -nostdlib -fno-builtin -s \
          -Wno-builtin-macro-redefined -DPRINTF_INCLUDE_CONFIG_H -D_KERNEL \
          $(INCLUDES)

ASFLAGS := --64

ARFLAGS := rcs

SRCS := sys_kern.c \
		sys_uname.c \
        sysinfo.c \
		kern_mutex.c

LIBKERN_SRCS := $(wildcard $(LIBKERNDIR)/*.c) $(LIBKERNDIR)/softfloat-support.c

ARCH_ASM_SRCS := $(wildcard $(ARCHDIR)/*.S)

OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)
LIBKERN_OBJS := $(LIBKERN_SRCS:$(LIBKERNDIR)/%.c=$(OBJDIR)/libkern_%.o)
ARCH_OBJS := $(ARCH_ASM_SRCS:$(ARCHDIR)/%.S=$(OBJDIR)/arch_%.o)

ALL_OBJS := $(OBJS) $(LIBKERN_OBJS) $(ARCH_OBJS)

.PHONY: all
all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(ALL_OBJS)
	$(AR) $(ARFLAGS) $@ $(ALL_OBJS)
	@echo "Built $@ with $(words $(ALL_OBJS)) objects"

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/libkern_%.o: $(LIBKERNDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/arch_%.o: $(ARCHDIR)/%.S
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

.PHONY: rebuild
rebuild: clean all

.PHONY: install
install: $(TARGET)
	@echo "Installing libkern.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include/sys
	cp $(INCDIR)/kernel.h ../../include/sys/ 2>/dev/null || true
	@mkdir -p ../../include/libkern
	cp $(LIBKERNDIR)/libkern.h ../../include/libkern/ 2>/dev/null || true
	@echo "Installation complete"

.PHONY: help
help:
	@echo "Makefile for libkern.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libkern.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Sources:"
	@echo "  Core:     $(words $(SRCS)) files"
	@echo "  Libkern:  $(words $(LIBKERN_SRCS)) C files"
	@echo "  Arch asm: $(words $(ARCH_ASM_SRCS)) assembly files"

-include $(ALL_OBJS:.o=.d)