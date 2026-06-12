; userprog.s — embedded ring-3 demo programs.
;
; Copied by the kernel into user-mapped pages and executed in ring 3, so
; they must be position-independent (the call/pop trick recovers the load
; address). Each prints its message three times with a CPU-bound spin in
; between — long enough that the PIT preempts mid-spin and the two
; programs' output interleaves.

SPIN_ITERATIONS equ 10000000

section .text

global user_prog_a_start
global user_prog_a_end
user_prog_a_start:
    call .here
.here:
    pop edi
    add edi, msg_a - .here     ; edi = runtime address of the message
    mov esi, 3                 ; iterations
.loop:
    mov eax, 1                 ; SYS_WRITE
    mov ebx, edi
    int 0x80
    mov ecx, SPIN_ITERATIONS   ; burn CPU so the timer preempts us
.spin:
    dec ecx
    jnz .spin
    dec esi
    jnz .loop
    mov eax, 0                 ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang                  ; unreachable
msg_a:
    db 'tick from process A', 0x0A, 0
user_prog_a_end:

global user_prog_b_start
global user_prog_b_end
user_prog_b_start:
    call .here
.here:
    pop edi
    add edi, msg_b - .here
    mov esi, 3
.loop:
    mov eax, 1                 ; SYS_WRITE
    mov ebx, edi
    int 0x80
    mov ecx, SPIN_ITERATIONS
.spin:
    dec ecx
    jnz .spin
    dec esi
    jnz .loop
    mov eax, 0                 ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang
msg_b:
    db 'tick from process B', 0x0A, 0
user_prog_b_end:
