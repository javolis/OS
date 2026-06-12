; usermode.s — ring 0 <-> ring 3 transitions.
;
; enter_user_mode irets into ring 3 with user selectors; the exit syscall
; later calls kernel_resume, which abandons the interrupt frame and returns
; to enter_user_mode's caller as if the function had returned normally.

section .bss
align 4
saved_kernel_esp:
    resd 1

section .text

; void enter_user_mode(uint32_t entry, uint32_t user_stack_top)
global enter_user_mode
enter_user_mode:
    push ebx                   ; preserve callee-saved registers so
    push esi                   ; kernel_resume can restore them
    push edi
    push ebp
    mov [saved_kernel_esp], esp

    mov ecx, [esp + 20]        ; entry point
    mov edx, [esp + 24]        ; user stack top

    mov ax, 0x23               ; user data selector (RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword 0x23            ; ss
    push edx                   ; esp
    pushfd
    pop eax
    or  eax, 0x200             ; IF set when user code starts (IOPL stays 0)
    push eax                   ; eflags
    push dword 0x1B            ; cs (user code, RPL 3)
    push ecx                   ; eip
    iretd

; void kernel_resume(void) — never returns to its caller; resumes the
; kernel context saved by enter_user_mode. Runs with kernel segments
; already loaded (the ISR stub did that).
global kernel_resume
kernel_resume:
    mov esp, [saved_kernel_esp]
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
