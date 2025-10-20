SYS_DIR := sys/src
BOOT_DIR := $(SYS_DIR)/boot
LIBCHAR_DIR := $(SYS_DIR)/libchar
BUILD_DIR := build
ISODIR := $(BUILD_DIR)/iso_root
ISO := $(BUILD_DIR)/uesi.iso

.PHONY: all
all: $(BUILD_DIR)/kernel.elf

.PHONY: libchar boot
libchar:
	@echo "[*] Building libchar..."
	@$(MAKE) -C $(LIBCHAR_DIR)

boot:
	@echo "[*] Building kernel..."
	@$(MAKE) -C $(BOOT_DIR)

$(BUILD_DIR)/kernel.elf: libchar boot
	@mkdir -p $(BUILD_DIR)
	@cp $(BOOT_DIR)/kernel.elf $(BUILD_DIR)/kernel.elf
	@echo "[+] Kernel built and copied to $(BUILD_DIR)/kernel.elf"

.PHONY: iso
iso: all
	@echo "[*] Creating ISO image..."
	@mkdir -p $(ISODIR)/boot/limine
	@cp $(BUILD_DIR)/kernel.elf $(ISODIR)/boot/uesi
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
	@$(MAKE) -C $(LIBCHAR_DIR) clean || true
	@$(MAKE) -C $(BOOT_DIR) clean || true
	@rm -rf $(BUILD_DIR)
	@echo "[+] Cleanup complete."

.PHONY: rebuild
rebuild: clean all

.PHONY: info
info:
	@echo "libchar dir: $(LIBCHAR_DIR)"
	@echo "boot dir:    $(BOOT_DIR)"
	@echo "build dir:   $(BUILD_DIR)"
	@echo "ISO path:    $(ISO)"
	@echo ""
	@echo "Targets: all, iso, run, run-kvm, debug, clean, rebuild, info"
