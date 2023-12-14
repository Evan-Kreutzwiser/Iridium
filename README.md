# Iridium

Iridium is a hobby operating system written from scratch in C, with a microkernel architecture for the x86_64 platform. It is inspired by Fuchsia's kernel object and capability system for handling processes' access to resources.

## Building

Building Iridium does not require many tools, just `gcc` and `make`, included in most distro's `build-essential` or `base-devel` packages. Additionally, grub is used to produce bootable disk images, which can be used in any x86_64 emulator of your choice.

If you have `qemu-system-x86_64` installed, you can test the OS easily using `make emu`.
