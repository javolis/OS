/* linker.ld — link the kernel as a freestanding ELF loaded at 1 MiB. */
ENTRY(_start)

SECTIONS
{
    /* GRUB loads the kernel at 1 MiB, a conventional spot above low memory. */
    . = 1M;

    /* The multiboot header must come early so GRUB can find it. */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
