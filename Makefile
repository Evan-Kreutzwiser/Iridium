# Set to the mount point of the partition / disk image to install the OS to
INSTALL_ROOT_DIR ?=

.PHONY: all kernel init install clean test docs

all: kernel init

kernel:
	(cd ./kernel; make)

init:
	(cd ./init; make)

clean:
	(cd ./kernel; make clean)
	(cd ./init; make clean)

iso: kernel init
	cp -f kernel/kernel.sys grub/kernel.sys
	cp -f init/init.sys grub/initrd.sys
	grub-mkrescue grub/ -o grub.img

emu: iso
	qemu-system-x86_64 -hda grub.img -serial file:serial.txt -no-reboot -m 1G -s -no-shutdown -d int

docs:
	doxygen

install:
