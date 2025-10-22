CC ?= x86_64-elf-gcc
AR := ar
CFLAGS := -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections \
          -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel -O2 -I$(INCDIR) -I../boot -nostdlib -fno-builtin -s
ARFLAGS := rcs
INCDIR := include
SRCDIR := string
OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libc.a
SRCS := $(SRCDIR)/malloc.c \
        $(SRCDIR)/memchr.c \
        $(SRCDIR)/memcmp.c \
        $(SRCDIR)/memcpy.c \
        $(SRCDIR)/memmove.c \
        $(SRCDIR)/memset.c \
        $(SRCDIR)/strcat.c \
        $(SRCDIR)/strchr.c \
        $(SRCDIR)/strcmp.c \
        $(SRCDIR)/strcoll.c \
        $(SRCDIR)/strcpy.c \
        $(SRCDIR)/strcspn.c \
        $(SRCDIR)/strerror.c \
        $(SRCDIR)/strerror_r.c \
        $(SRCDIR)/strlen.c \
        $(SRCDIR)/strncat.c \
        $(SRCDIR)/strncmp.c \
        $(SRCDIR)/strncpy.c \
        $(SRCDIR)/strndup.c \
        $(SRCDIR)/strpbrk.c \
        $(SRCDIR)/strrchr.c \
        $(SRCDIR)/strsignal.c \
        $(SRCDIR)/strspn.c \
        $(SRCDIR)/strstr.c \
        $(SRCDIR)/strtok.c \
        $(SRCDIR)/strtok_r.c \
        $(SRCDIR)/strxfrm.c
OBJS := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
INCLUDES := -I$(INCDIR) -I../boot
.PHONY: all
all: $(TARGET)
$(OBJDIR):
	@mkdir -p $(OBJDIR)
$(LIBDIR):
	@mkdir -p $(LIBDIR)
$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@"
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"
.PHONY: rebuild
rebuild: clean all
.PHONY: install
install: $(TARGET)
	@echo "Installing libc.a..."
	@mkdir -p ../../lib
	cp $(TARGET) ../../lib/
	@mkdir -p ../../include
	cp $(INCDIR)/string.h ../../include/
	@echo "Installation complete"
.PHONY: help
help:
	@echo "Makefile for libc.a"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libc.a (default)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  install  - Install library to ../../lib and headers to ../../include"
	@echo "  help     - Show this help message"
-include $(OBJS:.o=.d)
$(OBJDIR)/%.d: $(SRCDIR)/%.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJDIR)/$*.o $< > $@