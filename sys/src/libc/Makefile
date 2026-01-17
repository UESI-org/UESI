CC ?= x86_64-elf-gcc
AR := ar
ARFLAGS := rcs

INCDIR := include
SYSINCDIR := ../../../sys
SYSINCDIR1 := ../../../sys/sys
ARCHINCDIR := ../../../amd64/include
BOOTINCDIR := ../boot/include

SRCDIR := string
GENDIR := gen
SOFTFLOATDIR := softfloat
STDLIBDIR := stdlib
STDIODIR := stdio
TIMEDIR := time
MATHDIR := math
DIRENTDIR := dirent

OBJDIR := obj
LIBDIR := lib
TARGET := $(LIBDIR)/libc.a

SRCS := $(wildcard $(SRCDIR)/*.c)
GENSRC := $(wildcard $(GENDIR)/*.c)
SOFTFLOATSRC := $(wildcard $(SOFTFLOATDIR)/*.c)
STDLIBSRC := $(wildcard $(STDLIBDIR)/*.c)
STDIOSRC := $(wildcard $(STDIODIR)/*.c)
TIMESRC := $(wildcard $(TIMEDIR)/*.c)
MATHSRC := $(wildcard $(MATHDIR)/*.c)
DIRENTSRC := $(wildcard $(DIRENTDIR)/*.c)

OBJS := \
	$(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
	$(GENSRC:$(GENDIR)/%.c=$(OBJDIR)/gen_%.o) \
	$(SOFTFLOATSRC:$(SOFTFLOATDIR)/%.c=$(OBJDIR)/softfloat_%.o) \
	$(STDLIBSRC:$(STDLIBDIR)/%.c=$(OBJDIR)/stdlib_%.o) \
	$(STDIOSRC:$(STDIODIR)/%.c=$(OBJDIR)/stdio_%.o) \
	$(TIMESRC:$(TIMEDIR)/%.c=$(OBJDIR)/time_%.o) \
	$(MATHSRC:$(MATHDIR)/%.c=$(OBJDIR)/math_%.o) \
	$(DIRENTSRC:$(DIRENTDIR)/%.c=$(OBJDIR)/dirent_%.o)

FEATURE_FLAGS := \
	-D_POSIX_C_SOURCE=202405L \
	-D_XOPEN_SOURCE=700 \
	-D_BSD_SOURCE \
	-DSOFTFLOAT_FOR_GCC

CPPFLAGS := $(FEATURE_FLAGS)

CFLAGS := \
	-Wall -Wextra -std=gnu11 \
	-ffreestanding \
	-fno-stack-protector -fno-stack-check \
	-fno-lto -fno-PIC \
	-ffunction-sections -fdata-sections \
	-m64 -march=x86-64 -mabi=sysv \
	-mno-red-zone -mcmodel=kernel \
	-O2 \
	$(CPPFLAGS) \
	-I$(INCDIR) \
	-I$(SYSINCDIR) \
	-I$(SYSINCDIR1) \
	-I$(ARCHINCDIR) \
	-I$(BOOTINCDIR) \
	-I$(SOFTFLOATDIR) \
	-nostdlib -fno-builtin -DNDEBUG \
	-s

.PHONY: all clean rebuild install help

all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(TARGET): $(OBJDIR) $(LIBDIR) $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo "Built $@ ($(words $(OBJS)) objects)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/gen_%.o: $(GENDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/softfloat_%.o: $(SOFTFLOATDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/stdlib_%.o: $(STDLIBDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/stdio_%.o: $(STDIODIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/time_%.o: $(TIMEDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/math_%.o: $(MATHDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

$(OBJDIR)/dirent_%.o: $(DIRENTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	@echo "Cleaned build artifacts"

rebuild: clean all

install: $(TARGET)
	@echo "Installing libc..."
	@mkdir -p ../../lib ../../include

	cp $(TARGET) ../../lib/

	cp $(INCDIR)/string.h ../../include/
	cp $(INCDIR)/stdio.h ../../include/
	cp $(INCDIR)/stdlib.h ../../include/
	cp $(INCDIR)/math.h ../../include/
	cp $(INCDIR)/dirent.h ../../include/

	@if [ -f $(SOFTFLOATDIR)/softfloat.h ]; then \
		cp $(SOFTFLOATDIR)/softfloat.h ../../include/; \
		echo "Installed softfloat.h"; \
	fi

	@echo "Installation complete"

help:
	@echo "Freestanding libc Makefile"
	@echo ""
	@echo "Modules:"
	@echo "  string stdlib stdio time math gen softfloat"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build libc.a"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Clean + build"
	@echo "  install  - Install libc and headers"
	@echo "  help     - Show this message"

-include $(OBJS:.o=.d)
