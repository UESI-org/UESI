CC ?= x86_64-elf-gcc
AR := ar
ARFLAGS := rcs
INCDIR := include
SYSINCDIR := ../../../sys
ARCHINCDIR := ../../../amd64/include
BOOTINCDIR := ../boot/include
SRCDIR := string
GENDIR := gen
SOFTFLOATDIR := softfloat
STDLIBDIR := stdlib
OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libc.a

SRCS := $(wildcard $(SRCDIR)/*.c)
GENSRC := $(wildcard $(GENDIR)/*.c)
SOFTFLOATSRC := $(wildcard $(SOFTFLOATDIR)/*.c)
STDLIBSRC := $(wildcard $(STDLIBDIR)/*.c)

OBJS := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
        $(GENSRC:$(GENDIR)/%.c=$(OBJDIR)/gen_%.o) \
        $(SOFTFLOATSRC:$(SOFTFLOATDIR)/%.c=$(OBJDIR)/softfloat_%.o) \
        $(STDLIBSRC:$(STDLIBDIR)/%.c=$(OBJDIR)/stdlib_%.o)

FEATURE_FLAGS := -D__POSIX_VISIBLE=200809 \
                 -D__XPG_VISIBLE=700 \
                 -D__ISO_C_VISIBLE=2011 \
                 -D_BSD_SOURCE \
                 -DNL_TEXTMAX=128 \
                 -D_KERNEL \
                 -DSOFTFLOAT_FOR_GCC

CPPFLAGS := $(FEATURE_FLAGS)

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
    -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
    -m64 -march=x86-64 -mabi=sysv \
    -mno-red-zone -mcmodel=kernel -O2 \
    $(CPPFLAGS) \
    -I$(INCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) -I$(BOOTINCDIR) -I$(SOFTFLOATDIR) \
    -nostdlib -fno-builtin -s

INCLUDES := -I$(INCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) -I$(BOOTINCDIR) -I$(SOFTFLOATDIR)

.PHONY: all clean rebuild install help

all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@ with $(words $(OBJS)) object files"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/gen_%.o: $(GENDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/softfloat_%.o: $(SOFTFLOATDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/stdlib_%.o: $(STDLIBDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

rebuild: clean all

install: $(TARGET)
	@echo "Installing libc.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/string.h ../../include/
	@if [ -f $(SOFTFLOATDIR)/softfloat.h ]; then \
		cp $(SOFTFLOATDIR)/softfloat.h ../../include/; \
		echo "Installed softfloat.h"; \
	fi
	@echo "Installation complete"

help:
	@echo "Makefile for libc.a (with softfloat support)"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libc.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library and headers"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Feature visibility flags:"
	@echo "  __POSIX_VISIBLE=200809  - POSIX.1-2008"
	@echo "  __XPG_VISIBLE=700       - X/Open 7 (XSI)"
	@echo "  __ISO_C_VISIBLE=2011    - ISO C11"
	@echo "  __BSD_VISIBLE=1         - BSD extensions"
	@echo "  _KERNEL                 - Kernel build"
	@echo "  SOFTFLOAT_FOR_GCC       - GCC softfloat compatibility"
	@echo ""
	@echo "Components:"
	@echo "  - string functions ($(SRCDIR)/)"
	@echo "  - general functions ($(GENDIR)/)"
	@echo "  - softfloat library ($(SOFTFLOATDIR)/)"

-include $(OBJS:.o=.d)