ORG 0
; Rn
MOVB R0, R1
; (Rn)
MOVB (R2), R3
; (Rn)+
MOVB (R0)+, R1
; @(Rn)+
MOVB @(R2)+, R3
; -(Rn)
MOVB -(R4), R5
; @-(Rn)
MOVB @-(SP), R0
; X(Rn)
MOVB 02(R1), R2
; @X(Rn)
MOVB @04(R3), R4
; #imm
MOVB #0123, R5
; @#addr
MOVB @#0100, R0
