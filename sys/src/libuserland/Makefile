CC ?= x86_64-elf-gcc
AR := ar
ARFLAGS := rcs

INCDIR := include
BOOTINCDIR := ../boot/include
SYSINCDIR := ../../../sys
ARCHINCDIR := ../../../amd64/include
LIBMEMINCDIR := ../libmem/include
LIBCINCDIR := ../libc/include
LIBPRINTFINCDIR := ../libprintf/include
OBJDIR := obj
LIBDIR := lib

TARGET := $(LIBDIR)/libuserland.a
SRCS := elf_loader.c proc.c malloc.c
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

FEATURE_FLAGS := -D__POSIX_VISIBLE=200809 \
                 -D__XPG_VISIBLE=700 \
                 -D__ISO_C_VISIBLE=2011 \
                 -D__BSD_VISIBLE=1 \
                 -D_BSD_SOURCE \
                 -DNL_TEXTMAX=128 \
                 -D_KERNEL

CPPFLAGS := $(FEATURE_FLAGS)

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 \
          $(CPPFLAGS) \
          -nostdlib -fno-builtin -s

INCLUDES := -I$(INCDIR) -I$(BOOTINCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) \
            -I$(LIBMEMINCDIR) -I$(LIBCINCDIR) -I$(LIBPRINTFINCDIR)

.PHONY: all clean rebuild install help

all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@ with $(words $(OBJS)) object files"

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

rebuild: clean all

install: $(TARGET)
	@echo "Installing libuserland.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/*.h ../../include/
	@echo "Installation complete"

help:
	@echo "Makefile for libuserland.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libuserland.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Feature visibility flags:"
	@echo "  __POSIX_VISIBLE=200809  - POSIX.1-2008"
	@echo "  __XPG_VISIBLE=700       - X/Open 7 (XSI)"
	@echo "  __ISO_C_VISIBLE=2011    - ISO C11"
	@echo "  __BSD_VISIBLE=1         - BSD extensions"
	@echo "  _KERNEL                 - Kernel build"
	@echo ""
	@echo "Components:"
	@echo "  - ELF loader"
	@echo "  - Process management"
	@echo "  - Memory allocation (malloc)"

-include $(OBJS:.o=.d)