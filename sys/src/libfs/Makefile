CC ?= x86_64-elf-gcc
AR := ar
ARFLAGS := rcs

INCDIR := include
BOOTINCDIR := ../boot/include
SYSINCDIR := ../../../sys
ARCHINCDIR := ../../../amd64/include
LIBMEMINCDIR := ../libmem/include
LIBCINCDIR := ../libc/include
OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libfs.a

SRCS := vfs.c inode.c blk.c blkalloc.c tmpfs.c
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 -nostdlib -fno-builtin -s

INCLUDES := -I$(INCDIR) -I$(BOOTINCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) \
            -I$(LIBMEMINCDIR) -I$(LIBCINCDIR)

.PHONY: all clean rebuild install help

all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@"

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

rebuild: clean all

install: $(TARGET)
	@echo "Installing libfs.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/*.h ../../include/
	@echo "Installation complete"

help:
	@echo "Makefile for libfs.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libfs.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"

-include $(OBJS:.o=.d)