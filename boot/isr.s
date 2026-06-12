; isr.s — CPU exception entry stubs (vectors 0-31).
;
; Some exceptions push an error code, the rest don't; the no-error stubs push
; a dummy 0 so every vector reaches isr_common with an identical stack frame
; (matching struct registers in include/idt.h).

section .text

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0               ; dummy error code
    push dword %1              ; interrupt number
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:                         ; CPU already pushed the error code
    push dword %1              ; interrupt number
    jmp isr_common
%endmacro

ISR_NOERR 0                    ; divide error
ISR_NOERR 1                    ; debug
ISR_NOERR 2                    ; non-maskable interrupt
ISR_NOERR 3                    ; breakpoint
ISR_NOERR 4                    ; overflow
ISR_NOERR 5                    ; BOUND range exceeded
ISR_NOERR 6                    ; invalid opcode
ISR_NOERR 7                    ; device not available
ISR_ERR   8                    ; double fault
ISR_NOERR 9                    ; coprocessor segment overrun (legacy)
ISR_ERR   10                   ; invalid TSS
ISR_ERR   11                   ; segment not present
ISR_ERR   12                   ; stack-segment fault
ISR_ERR   13                   ; general protection fault
ISR_ERR   14                   ; page fault
ISR_NOERR 15                   ; reserved
ISR_NOERR 16                   ; x87 floating-point exception
ISR_ERR   17                   ; alignment check
ISR_NOERR 18                   ; machine check
ISR_NOERR 19                   ; SIMD floating-point exception
ISR_NOERR 20                   ; virtualization exception
ISR_ERR   21                   ; control protection exception
ISR_NOERR 22                   ; reserved
ISR_NOERR 23                   ; reserved
ISR_NOERR 24                   ; reserved
ISR_NOERR 25                   ; reserved
ISR_NOERR 26                   ; reserved
ISR_NOERR 27                   ; reserved
ISR_NOERR 28                   ; hypervisor injection exception
ISR_ERR   29                   ; VMM communication exception
ISR_ERR   30                   ; security exception
ISR_NOERR 31                   ; reserved

; Hardware IRQ stubs (vectors 32-47, after the 8259 PIC remap). IRQs never
; push an error code, so always push a dummy 0 to match struct registers.
%macro IRQ 1
global irq%1
irq%1:
    push dword 0               ; dummy error code
    push dword (32 + %1)       ; vector number
    jmp irq_common
%endmacro

IRQ 0                          ; PIT timer
IRQ 1                          ; PS/2 keyboard
IRQ 2                          ; cascade (never raised directly)
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

extern irq_handler
irq_common:
    pusha                      ; edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds
    push eax                   ; save interrupted data segment

    mov ax, 0x10               ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                   ; struct registers * argument
    call irq_handler
    add esp, 4

    pop eax                    ; restore interrupted data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8                 ; drop vector number and error code
    iret

extern isr_handler
isr_common:
    pusha                      ; edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds
    push eax                   ; save interrupted data segment

    mov ax, 0x10               ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                   ; struct registers * argument
    call isr_handler
    add esp, 4

    pop eax                    ; restore interrupted data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8                 ; drop interrupt number and error code
    iret
