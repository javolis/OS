; usermode.s — ring-3 entry and kernel context switching.

section .text

; void user_iret(uint32_t entry, uint32_t user_stack_top)
; Builds a ring-3 iret frame (user selectors, IF set, IOPL 0) and enters
; user mode. Does not return; the task thereafter re-enters the kernel
; only via interrupts and syscalls.
global user_iret
user_iret:
    mov ecx, [esp + 4]         ; entry point
    mov edx, [esp + 8]         ; user stack top

    mov ax, 0x23               ; user data selector (RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword 0x23            ; ss
    push edx                   ; esp
    pushfd
    pop eax
    or  eax, 0x200             ; IF set when user code starts
    push eax                   ; eflags
    push dword 0x1B            ; cs (user code, RPL 3)
    push ecx                   ; eip
    iretd

; void switch_context(uint32_t *save_esp, uint32_t new_esp)
; Park the current kernel context (callee-saved registers + esp) and
; resume another. A fresh task's forged stack makes the final ret land
; in task_entry; a parked task resumes inside schedule() and unwinds
; back through whatever interrupt frame suspended it.
global switch_context
switch_context:
    mov eax, [esp + 4]         ; &prev->kesp
    mov edx, [esp + 8]         ; next->kesp
    push ebx
    push esi
    push edi
    push ebp
    mov [eax], esp
    mov esp, edx
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
