; userblobs.s — userland ELF executables embedded in the kernel image.
; There is no filesystem yet, so the compiled user programs ride along in
; .rodata and the kernel's ELF loader stages them into address spaces.

section .rodata

global user_elf_a_start
global user_elf_a_end
user_elf_a_start:
    incbin "user/hello_a.elf"
user_elf_a_end:

global user_elf_b_start
global user_elf_b_end
user_elf_b_start:
    incbin "user/hello_b.elf"
user_elf_b_end:
