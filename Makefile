# Set to the mount point of the partition / disk image to install the OS to
INSTALL_ROOT_DIR ?=

.PHONY: all kernel init libc install clean test docs

all: kernel init libc

kernel:
	(cd ./kernel; make)

init: libc
	(cd ./init; make)

libc:
	(cd ./libc; make)

clean:
	(cd ./kernel; make clean)
	(cd ./init; make clean)

iso: kernel init
	cp -f kernel/kernel.sys grub/kernel.sys
	cp -f init/init.sys grub/initrd.sys
	grub-mkrescue grub/ -o grub.img

emu: iso
	qemu-system-x86_64 -hda grub.img -serial file:serial.txt -no-reboot -m 1G -s -no-shutdown

docs:
	doxygen

install: kernel init
	cp kernel/kernel.sys /mnt/c/boot/kernel.sys
	cp init/init.sys /mnt/c/boot/initrd.sys
