.data
    bitmap:
    dd <font.bin>

    string:
    dd 6, 120, 207, 228, 228, 237, 3

.text
mov %ex, string
mov %sp, 0x300
mov %cx, 0xffff
mov %ax, 0x8101
mov %dx, 0
mov (0x5), %dx

outloop1:
    mov %bx, [%ex]
    add %bx, bitmap

    push %ax
    push %bx
    push %cx
    push %dx
    push %ex

    call print_char

    pop %ex
    pop %dx
    pop %cx
    pop %bx
    pop %ax

    inc %ex
    inc (0x5)

    add %ax, 6
    mov %dx, 6
    cmp %dx, (0x5)
    jnz outloop1

swp
forever:
jmp forever


print_char:
    mov %dx, 0
    mov (0x0), %dx

    sub %ax, 1

    forloop1:
        mov %dx, 0
        mov (0x1), %dx

        mov %dx, 1

        forloop2:
            mov %ex, 0
            mov (0x2), %ex
            add %ax, 5
            forloop3:

                mov %ex, [%bx]
                and %ex, %dx
                jnz write

                if1:
                shl %dx
                dec %ax

                inc (0x2)
                mov %ex, 5
                cmp %ex, (0x2)
                jnz forloop3


            add %ax, 256
            inc (0x1)
            mov %ex, 3
            cmp %ex, (0x1)
            jnz forloop2


        inc %bx
        inc (0x0)
        mov %dx, 3
        cmp %dx, (0x0)
        jnz forloop1
    ret


write:
    mov (%ax), %cx
    jmp if1