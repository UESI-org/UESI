export CC := x86_64-elf-gcc
export LD := x86_64-elf-ld
export AR := ar

SYS_DIR := sys/src
USER_DIR := user
USERLAND_PROG := $(USER_DIR)/bin/test

BOOT_DIR := $(SYS_DIR)/boot
LIBCHAR_DIR := $(SYS_DIR)/libchar
LIBC_DIR := $(SYS_DIR)/libc
LIBKB_DIR := $(SYS_DIR)/libkb
LIBMEM_DIR := $(SYS_DIR)/libmem
LIBPRINTF_DIR := $(SYS_DIR)/libprintf
LIBKDEBUG_DIR := $(SYS_DIR)/libkdebug
LIBUSERLAND_DIR := $(SYS_DIR)/libuserland
LIBFS_DIR := $(SYS_DIR)/libfs
LIBKERN_DIR := sys/kern

BUILD_DIR := build
ISODIR := $(BUILD_DIR)/iso_root
ISO := $(BUILD_DIR)/uesi.iso

.PHONY: all
all: $(BUILD_DIR)/uesi.elf

.NOTPARALLEL: libc libchar libkb libmem libprintf libkdebug libuserland libfs libkern

libc:
	@echo "[*] Building libc..."
	@$(MAKE) -C $(LIBC_DIR) CC=$(CC) AR=$(AR)

libchar:
	@echo "[*] Building libchar..."
	@$(MAKE) -C $(LIBCHAR_DIR) CC=$(CC) AR=$(AR)

libkb:
	@echo "[*] Building libkb..."
	@$(MAKE) -C $(LIBKB_DIR) CC=$(CC) AR=$(AR)

libmem:
	@echo "[*] Building libmem..."
	@$(MAKE) -C $(LIBMEM_DIR) CC=$(CC) AR=$(AR)

libprintf:
	@echo "[*] Building libprintf..."
	@$(MAKE) -C $(LIBPRINTF_DIR) CC=$(CC) AR=$(AR)

libkdebug:
	@echo "[*] Building libkdebug..."
	@$(MAKE) -C $(LIBKDEBUG_DIR) CC=$(CC) AR=$(AR)

libuserland:
	@echo "[*] Building libuserland..."
	@$(MAKE) -C $(LIBUSERLAND_DIR) CC=$(CC) AR=$(AR)

libfs:
	@echo "[*] Building libfs..."
	@$(MAKE) -C $(LIBFS_DIR) CC=$(CC) AR=$(AR)

libkern:
	@echo "[*] Building libkern..."
	@$(MAKE) -C $(LIBKERN_DIR) CC=$(CC) AR=$(AR)

boot: libc libchar libkb libmem libprintf libkdebug libuserland libfs libkern
	@echo "[*] Building kernel..."
	@$(MAKE) -C $(BOOT_DIR) CC=$(CC) LD=$(LD)

.PHONY: userland
userland:
	@echo "[*] Building userland programs..."
	@$(MAKE) -C $(USER_DIR)/src \
	CC=$(CC) \
	LD=$(LD) \
	AR=$(AR)

$(USERLAND_PROG):
	@echo "[*] Building userland program..."
	@$(MAKE) -C $(USER_DIR) CC=$(CC) LD=$(LD) AR=$(AR)

$(BUILD_DIR)/uesi.elf: boot
	@mkdir -p $(BUILD_DIR)
	@cp $(BOOT_DIR)/uesi.elf $(BUILD_DIR)/uesi.elf
	@echo "[+] Kernel built and copied to $(BUILD_DIR)/uesi.elf"

.PHONY: iso
iso: all $(USERLAND_PROG)
	@echo "[*] Creating ISO image..."
	@mkdir -p $(ISODIR)/boot/limine
	@cp $(BUILD_DIR)/uesi.elf $(ISODIR)/boot/uesi
	@cp $(USER_DIR)/bin/test $(ISODIR)/boot/test_program
	@cp $(BOOT_DIR)/limine.conf $(ISODIR)/boot/limine/
	@if [ -f /usr/share/limine/limine-bios.sys ]; then \
		cp /usr/share/limine/limine-bios.sys $(ISODIR)/boot/limine/; \
	elif [ -f /usr/local/share/limine/limine-bios.sys ]; then \
		cp /usr/local/share/limine/limine-bios.sys $(ISODIR)/boot/limine/; \
	else \
		echo "Error: limine-bios.sys not found"; exit 1; \
	fi
	@if [ -f /usr/share/limine/limine-bios-cd.bin ]; then \
		cp /usr/share/limine/limine-bios-cd.bin $(ISODIR)/boot/limine/; \
	elif [ -f /usr/local/share/limine/limine-bios-cd.bin ]; then \
		cp /usr/local/share/limine/limine-bios-cd.bin $(ISODIR)/boot/limine/; \
	else \
		echo "Error: limine-bios-cd.bin not found"; exit 1; \
	fi
	@if [ -f /usr/share/limine/limine-uefi-cd.bin ]; then \
		cp /usr/share/limine/limine-uefi-cd.bin $(ISODIR)/boot/limine/; \
	elif [ -f /usr/local/share/limine/limine-uefi-cd.bin ]; then \
		cp /usr/local/share/limine/limine-uefi-cd.bin $(ISODIR)/boot/limine/; \
	else \
		echo "Error: limine-uefi-cd.bin not found"; exit 1; \
	fi
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISODIR) -o $(ISO)
	@echo "[+] ISO created: $(ISO)"

.PHONY: run run-kvm debug
run: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -serial stdio

run-kvm: iso
	qemu-system-x86_64 -enable-kvm -cdrom $(ISO) -m 256M -serial stdio

debug: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -serial stdio -s -S

.PHONY: clean
clean:
	@echo "[*] Cleaning all components..."
	@$(MAKE) -C $(LIBC_DIR) clean || true
	@$(MAKE) -C $(LIBCHAR_DIR) clean || true
	@$(MAKE) -C $(LIBKB_DIR) clean || true
	@$(MAKE) -C $(LIBMEM_DIR) clean || true
	@$(MAKE) -C $(LIBPRINTF_DIR) clean || true
	@$(MAKE) -C $(LIBKDEBUG_DIR) clean || true
	@$(MAKE) -C $(LIBUSERLAND_DIR) clean || true
	@$(MAKE) -C $(LIBFS_DIR) clean || true
	@$(MAKE) -C $(LIBKERN_DIR) clean || true
	@$(MAKE) -C $(BOOT_DIR) clean || true
	@$(MAKE) -C $(USER_DIR) clean || true
	@$(MAKE) -C $(USER_DIR)/src clean || true
	@rm -rf $(BUILD_DIR)
	@echo "[+] Cleanup complete."

.PHONY: rebuild
rebuild: clean all

.PHONY: info
info:
	@echo "libc dir:       $(LIBC_DIR)"
	@echo "libchar dir:    $(LIBCHAR_DIR)"
	@echo "libkb dir:      $(LIBKB_DIR)"
	@echo "libmem dir:     $(LIBMEM_DIR)"
	@echo "libprintf dir:  $(LIBPRINTF_DIR)"
	@echo "libkdebug dir:  $(LIBKDEBUG_DIR)"
	@echo "libuserland dir:$(LIBUSERLAND_DIR)"
	@echo "libfs dir:      $(LIBFS_DIR)"
	@echo "libkern dir:    $(LIBKERN_DIR)"
	@echo "boot dir:       $(BOOT_DIR)"
	@echo "build dir:      $(BUILD_DIR)"
	@echo "ISO path:       $(ISO)"
	@echo ""
	@echo "Targets: all, iso, run, run-kvm, debug, clean, rebuild, info"