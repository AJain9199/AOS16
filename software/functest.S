.data
dd 0x0

.text
mov %ax, 0x8000
mov %bx, 0x001f
call test_fn

mov %ax, 0x8001
mov %bx, 0xffff
call test_fn

swp

forever:
jmp forever

test_fn:
    mov (%ax), %bx
    ret