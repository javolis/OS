; userprog.s — embedded ring-3 demo program.
;
; Copied by the kernel into a user-mapped page and executed in ring 3, so
; it must be position-independent (the call/pop trick recovers the load
; address). It speaks to the kernel only via int 0x80.

section .text

global user_program_start
global user_program_end

user_program_start:
    call .here                 ; push the address of .here
.here:
    pop ebx
    add ebx, msg - .here       ; ebx = runtime address of msg
    mov eax, 1                 ; SYS_WRITE(string)
    int 0x80
    mov eax, 0                 ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang                  ; unreachable

msg:
    db 'Hello from ring 3 (write syscall)!', 0x0A, 0

user_program_end:
