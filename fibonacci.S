mov %ax, 0x1
mov %bx, 0x1

loop:
    mov %cx, %ax
    add %cx, %bx
    mov %bx, %ax
    mov %ax, %cx
    mov (0x0), %ax
    jmp loop