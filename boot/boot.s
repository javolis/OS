; boot.s — Multiboot header + higher-half entry point (i686)
; Assemble with: nasm -f elf32
;
; The kernel is linked at 0xC0100000 but loaded at 1 MiB physical. This file
; contains the only code that runs at its physical address (section .boot,
; linked low — see linker.ld): it builds a temporary page directory using
; 4 MiB PSE pages that maps the first 4 MiB both at 0 (identity, so the
; instruction after enabling paging still executes) and at 0xC0000000 (the
; higher-half view), turns paging on, and jumps high. paging_init later
; replaces this directory with proper 4 KiB page tables and drops the
; identity mapping.

MBALIGN  equ 1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1            ; provide memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002        ; Multiboot 1 magic number
CHECKSUM equ -(MAGIC + MBFLAGS)

KERNEL_VIRT_BASE equ 0xC0000000
KERNEL_PDE_INDEX equ (KERNEL_VIRT_BASE >> 22)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .boot
global _start
_start:
    ; EAX holds the multiboot magic and EBX the (physical) info pointer;
    ; both are forwarded to kernel_main untouched.

    mov ecx, cr4               ; allow 4 MiB pages for the boot mapping
    or  ecx, 0x10              ; CR4.PSE
    mov cr4, ecx

    ; present | writable | 4 MiB page, backed by physical 0..4 MiB
    mov dword [boot_page_directory], 0x00000083
    mov dword [boot_page_directory + KERNEL_PDE_INDEX * 4], 0x00000083

    mov ecx, boot_page_directory
    mov cr3, ecx

    mov ecx, cr0
    or  ecx, 0x80000000        ; CR0.PG
    mov cr0, ecx

    mov ecx, higher_half       ; absolute virtual address
    jmp ecx

align 4096
boot_page_directory:
    times 1024 dd 0

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB kernel stack
stack_top:

section .text
higher_half:
    mov esp, stack_top         ; stack at its virtual address

    push ebx                   ; multiboot info pointer (still physical)
    push eax                   ; bootloader magic
    extern kernel_main
    call kernel_main           ; kernel_main(magic, mbi_phys)

.hang:
    cli
    hlt
    jmp .hang
