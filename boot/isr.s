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
