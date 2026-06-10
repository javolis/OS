; boot.s — Multiboot header + kernel entry point (i686)
; Assemble with: nasm -f elf32

MBALIGN  equ 1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1            ; provide memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002        ; Multiboot 1 magic number
CHECKSUM equ -(MAGIC + MBFLAGS)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB kernel stack
stack_top:

section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top         ; set up the stack

    extern kernel_main
    call kernel_main           ; hand off to C

.hang:
    cli
    hlt
    jmp .hang
.end:
