CC ?= x86_64-elf-gcc
AR := ar

INCDIR := include
OBJDIR := obj
LIBDIR := lib

TARGET := $(LIBDIR)/libchar.a

INCLUDES := -I$(INCDIR) -I../boot/include -I../libc/include -I../.. -I../../../amd64/include

CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 -nostdlib -fno-builtin -s \
          $(INCLUDES)
ARFLAGS := rcs

SRCS := tty_init.c \
        tty_write.c \
        tty_cursor.c \
        tty_color.c \
        tty_screen.c \
        tty_font.c \
        tty_printf.c \
        tty_pixel.c

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
	@echo "Installing libchar.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/tty.h ../../include/
	@echo "Installation complete"

.PHONY: help
help:
	@echo "Makefile for libchar.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libchar.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"

-include $(OBJS:.o=.d)

$(OBJDIR)/%.d: %.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJDIR)/$*.o $< > $@