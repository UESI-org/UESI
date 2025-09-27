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
	# Makefile in src/
	echo "# Auto-generated Makefile in src/" > $(ROOTFS)/Makefile
	echo "all:" >> $(ROOTFS)/Makefile
	echo "\techo \"..."" >> $(ROOTFS)/Makefile

clean:
	rm -rf $(ROOTFS)
