CC ?= x86_64-elf-gcc
AR := ar

INCDIR := include
OBJDIR := obj
LIBDIR := lib

TARGET := $(LIBDIR)/libkdebug.a
INCLUDES := -I$(INCDIR) -I../boot/include -I../../../amd64/include -I../../../sys/sys
CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
	-fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
	-m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
	-mno-red-zone -mcmodel=kernel -O2 -nostdlib -fno-builtin -s \
	-I../libprintf/include \
	$(INCLUDES) \
	-nostdlib -fno-builtin -s -DPRINTF_INCLUDE_CONFIG_H

ARFLAGS := rcs
SRCS := kdebug.c
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all

all: $(TARGET)
$(OBJDIR):
	@mkdir -p $(OBJDIR)
$(LIBDIR):
	@mkdir -p $(LIBDIR)
$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@"
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

.PHONY: rebuild
rebuild: clean all

.PHONY: install
install: $(TARGET)
	@echo "Installing libkdebug.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/kdebug.h ../../include/
	@echo "Installation complete"

.PHONY: help
help:
	@echo "Makefile for libkdebug.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libkdebug.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"

-include $(OBJS:.o=.d)
$(OBJDIR)/%.d: %.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJDIR)/$*.o $< > $@