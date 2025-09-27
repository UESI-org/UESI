ROOTFS := src
.PHONY: all clean
all:
	mkdir -p $(ROOTFS)/bin
	mkdir -p $(ROOTFS)/sbin
	mkdir -p $(ROOTFS)/etc
	mkdir -p $(ROOTFS)/dev
	mkdir -p $(ROOTFS)/proc
	mkdir -p $(ROOTFS)/lib
	mkdir -p $(ROOTFS)/tmp
	mkdir -p $(ROOTFS)/usr/bin
	mkdir -p $(ROOTFS)/usr/lib
	mkdir -p $(ROOTFS)/var/log
	mkdir -p $(ROOTFS)/lib/modules/UESI/kernel/drivers
	@echo "# Generated Makefile for ROOTFS" > $(ROOTFS)/Makefile
	@echo ".PHONY: info clean-logs" >> $(ROOTFS)/Makefile
	@echo "" >> $(ROOTFS)/Makefile
	@echo "info:" >> $(ROOTFS)/Makefile
	@echo "	@echo \"POSIX filesystem layout created\"" >> $(ROOTFS)/Makefile
	@echo "	@echo \"Root directory: \$$(pwd)\"" >> $(ROOTFS)/Makefile
	@echo "	@find . -type d | head -20" >> $(ROOTFS)/Makefile
	@echo "" >> $(ROOTFS)/Makefile
	@echo "clean-logs:" >> $(ROOTFS)/Makefile
	@echo "	rm -f var/log/*" >> $(ROOTFS)/Makefile
	@echo "	@echo \"Log files cleared\"" >> $(ROOTFS)/Makefile
clean:
	rm -rf $(ROOTFS)
