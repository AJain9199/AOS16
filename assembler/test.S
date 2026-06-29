.data
    bitmap:
    dd 8, 0x6, 0x9, 0x9, 0xf, 0x9, 0x9, 0x9, 0x0
.text
    mov %ax, 0x8007
    mov %cx, 0xffff
    mov (0x0), %cx
    mov %cx, 0
    mov (0x2), %cx
    mov %bx, bitmap

    forloop1:
        mov %ex, %ax
        mov %cx, 0
        mov (0x1), %cx
        mov %cx, 1

        forloop2:
            mov %dx, [%bx]
            and %dx, %cx
            jnz write

            if1:
            dec %ex
            shl %cx

            inc (0x1)
            mov %dx, 8
            cmp %dx, (0x1)
            jnz forloop2


        inc %bx
        add %ax, 256

        inc (0x2)
        mov %cx, 0x8
        cmp %cx, (0x2)
        jnz forloop1

    swp

    forever:
        jmp forever

write:
    mov %dx, (0x0)
    mov (%ex), %dx
    jmp if1