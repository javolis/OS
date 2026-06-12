/* user.ld — userland executables: linked at the traditional 0x08048000.
 * The ALIGN keeps read-only and writable segments off shared pages (the
 * kernel loader copes either way, but clean segments are nicer). */
ENTRY(_start)

SECTIONS
{
    . = 0x08048000;

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }

    . = ALIGN(4096);

    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
