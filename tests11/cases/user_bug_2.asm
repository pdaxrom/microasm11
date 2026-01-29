        ORG  0200

start:
        mov #stack, sp
        mov #msg, r1           ; address of string
loop:
        movb (r1)+, r0         ; load byte
        beq end                ; zero terminator ends loop
        movb r0, @#177566      ; write to DL11 XBUF
        br loop
end:
        halt

msg:    db "Hello!", 0xa, 0

        ds 16
stack:
