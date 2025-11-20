CC ?= x86_64-elf-gcc
AR := ar
ARFLAGS := rcs

INCDIR := include
SYSINCDIR := ../../../sys
ARCHINCDIR := ../../../amd64/include
BOOTINCDIR := ../boot/include

SRCDIR := string
GENDIR := gen

OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libc.a

SRCS := $(wildcard $(SRCDIR)/*.c)
GENSRC := $(wildcard $(GENDIR)/*.c)

OBJS := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
        $(GENSRC:$(GENDIR)/%.c=$(OBJDIR)/gen_%.o)

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 \
          -I$(INCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) -I$(BOOTINCDIR) \
          -nostdlib -fno-builtin -s

INCLUDES := -I$(INCDIR) -I$(SYSINCDIR) -I$(ARCHINCDIR) -I$(BOOTINCDIR)

.PHONY: all clean rebuild install help

all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP

$(OBJDIR)/gen_%.o: $(GENDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP

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
	@echo "Installation complete"

help:
	@echo "Makefile for libc.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libc.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library and headers"
	@echo "  help     - Show this help message"

-include $(OBJS:.o=.d)
