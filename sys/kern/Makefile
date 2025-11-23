CC ?= x86_64-elf-gcc
AR := ar
INCDIR := ../sys
OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libkern.a
INCLUDES := -I.. -I../../amd64/include -I../src/libc/include -nostdinc
CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 -nostdlib -fno-builtin -s \
          $(INCLUDES)
ARFLAGS := rcs
SRCS := sys_kern.c \
        sysinfo.c
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
	@echo "Installing libkern.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include/sys
	cp $(INCDIR)/kernel.h ../../include/sys/
	@echo "Installation complete"

.PHONY: help
help:
	@echo "Makefile for libkern.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libkern.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include/sys"
	@echo "  help     - Show this help message"

-include $(OBJS:.o=.d)
$(OBJDIR)/%.d: %.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJDIR)/$*.o $< > $@