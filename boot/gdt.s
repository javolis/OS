; gdt.s — load the GDTR and reload segment registers (see kernel/gdt.c).
; Called as: gdt_flush(const struct gdt_ptr *gp)

section .text
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]         ; pointer to the gdt_ptr structure
    lgdt [eax]

    mov ax, 0x10               ; GDT_KERNEL_DATA: reload data segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush            ; GDT_KERNEL_CODE: far jump reloads CS
.flush:
    ret
