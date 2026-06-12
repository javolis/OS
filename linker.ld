/* linker.ld — higher-half kernel: linked at 0xC0100000, loaded at 1 MiB. */
ENTRY(_start)

SECTIONS
{
    /* Boot code runs with paging off, so it is linked AND loaded low.
     * The multiboot header must come early so GRUB can find it. */
    . = 1M;
    .boot ALIGN(4K) :
    {
        *(.multiboot)
        *(.boot)
    }

    /* The rest of the kernel is linked in the higher half but loaded
     * immediately after .boot (AT subtracts the virtual offset). */
    . += 0xC0000000;

    kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
    {
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
    {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
    {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}
