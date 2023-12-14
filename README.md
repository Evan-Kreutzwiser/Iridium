# Iridium

Iridium is a hobby operating system written from scratch in C, with a microkernel architecture for the x86_64 platform. It is inspired by Fuchsia's kernel object and capability system for handling processes' access to resources.

## Building

Building Iridium does not require many tools, just `make`, and an x86_64-elf-gcc cross compiler (Stock gcc on x86_64 may work, but is not recomended as this is not a linux clone). Additionally, grub is used to produce bootable disk images, which can be used in any x86_64 emulator of your choice.

To build the OS and init process, run `make`. `make iso` generates a bootable disk image named `grub.img`.

If you have `qemu-system-x86_64` installed, you can test the OS easily using `make emu`.

## Documentation

Limited kernel documentation is available in the `docs/` folder, outlining available system calls and how the OS works. However, the documentation is incomplete and the API is subject to change without warning.
