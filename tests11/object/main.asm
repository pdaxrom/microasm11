public start
extern ext_target
entry start

start:
    mov #local, r0
    mov #ext_target, r1
    jsr pc, ext_target
    mov local, r2
local:
    dw ext_target
