.data
    bitmap:
    dd <font.bin>

.text
    mov %ax, 0x8000
    mov %cx, 0xffff
    mov %bx, bitmap
    add %bx, 99
    mov %dx, 0
    mov (0x0), %dx

    mov %bx, 3

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

    swp
    forever:
    jmp forever


write:
    mov (%ax), %cx
    jmp if1