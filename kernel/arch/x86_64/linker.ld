ENTRY(multiboot_start)

KERNEL_PHYSICAL_ADDRESS = 0x100000;
KERNEL_VIRTUAL_ADDRESS = 0xFFFFFFFF80000000; /* -2 GB */

SECTIONS
{

    . = KERNEL_PHYSICAL_ADDRESS;
    _start_physical = .;
    _start = . + KERNEL_VIRTUAL_ADDRESS;
    /* .boot contains the portions of startup code that runs in the
        physical address space before transitioning to higher half
        aloong with bootloader headers */
    .boot :
    {
        _BOOT_START_ = .;
        KEEP(*(.multiboot))
        *(.boot)
        . = ALIGN(4096);
        _BOOT_END_ = .;
    }

    . += KERNEL_VIRTUAL_ADDRESS;

    .text : AT(ADDR(.text) - KERNEL_VIRTUAL_ADDRESS)
    {
        _TEXT_START_ = .;
        *(.text)
        . = ALIGN(4096);
        _TEXT_END_ = .;
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRTUAL_ADDRESS)
    {
        _RODATA_START_ = .;
        *(.rodata)
        . = ALIGN(4096);
        _RODATA_END_ = .;
    }

    .data : AT(ADDR(.data) - KERNEL_VIRTUAL_ADDRESS)
    {
        _DATA_START_ = .;
        *(.data)
        . = ALIGN(4096);
        _DATA_END_ = .;
    }

    KERNEL_PHYSICAL_DATA_END = . - KERNEL_VIRTUAL_ADDRESS;

    .bss : AT(ADDR(.bss) - KERNEL_VIRTUAL_ADDRESS)
    {
        _BSS_START_ = .;
        *(.bss)
        . = ALIGN(4096);
        _BSS_END_ = .;
    }

    /*bootstrap_heap_start = .;
    . += 4096 * 4; /* 16KB Bootstrap heap. Probably overkill, but better safe than sorry */
    /*bootstrap_heap_end = .;*/

    _end_physical = . - KERNEL_VIRTUAL_ADDRESS;
    _end = .;


    /DISCARD/ :
    {
        *(.comment)
    }
}
