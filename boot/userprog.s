; userprog.s — embedded ring-3 demo programs.
;
; Copied by the kernel into user-mapped pages and executed in ring 3, so
; they must be position-independent (the call/pop trick recovers the load
; address). They speak to the kernel only via int 0x80. Both are mapped at
; the same virtual address — in different address spaces.

section .text

%macro USER_PROG 2             ; %1 = label prefix, %2 = message label
global %1_start
global %1_end
%1_start:
    call %%here                ; push the address of %%here
%%here:
    pop ebx
    add ebx, %2 - %%here       ; ebx = runtime address of the message
    mov eax, 1                 ; SYS_WRITE(string)
    int 0x80
    mov eax, 0                 ; SYS_EXIT
    int 0x80
%%hang:
    jmp %%hang                 ; unreachable
%2:
%endmacro

USER_PROG user_prog_a, msg_a
    db 'process A says hello from ring 3 at 0x08048000!', 0x0A, 0
user_prog_a_end:

USER_PROG user_prog_b, msg_b
    db 'process B says hello from the same address, different page tables!', 0x0A, 0
user_prog_b_end:
