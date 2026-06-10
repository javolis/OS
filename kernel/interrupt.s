; interrupt.s — GDT/IDT loaders and the ISR/IRQ entry stubs (NASM, elf32).
bits 32

; ---------------------------------------------------------------------------
; void gdt_flush(uint32_t gdt_ptr_addr)
; Load the GDT and reload every segment register.
; ---------------------------------------------------------------------------
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10          ; 0x10 = kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush       ; 0x08 = kernel code selector; far jump reloads CS
.flush:
    ret

; ---------------------------------------------------------------------------
; void idt_flush(uint32_t idt_ptr_addr)
; ---------------------------------------------------------------------------
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; ---------------------------------------------------------------------------
; ISR stubs. Exceptions without an error code push a dummy 0 so the stack
; layout is uniform; those that push their own (8, 10-14, 17) do not.
; ---------------------------------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    cli
    push 0
    push %2
    jmp irq_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ---------------------------------------------------------------------------
; Common stubs: save state, switch to kernel data segment, call into C,
; then restore and return with iret.
; ---------------------------------------------------------------------------
extern isr_handler
isr_common_stub:
    pusha
    mov ax, ds
    push eax              ; save the data segment selector
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp              ; registers_t* argument
    call isr_handler
    add esp, 4
    pop eax               ; restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8            ; discard int_no and err_code
    iret

extern irq_handler
irq_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call irq_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    iret
