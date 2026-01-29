ORG 0
; Rn
MOV R0, R1
; (Rn) / @Rn
MOV (R2), R3
MOV @R4, R5
; (Rn)+
MOV (R0)+, R1
; @(Rn)+
MOV @(R2)+, R3
; -(Rn)
MOV -(R4), R5
; @-(Rn)
MOV @-(SP), R0
; X(Rn)
MOV 02(R1), R2
; @X(Rn)
MOV @04(R3), R4
; #imm
MOV #012345, R5
; @#addr
MOV @#0100, R0
; label(PC)
OFF EQU 6
MOV OFF(PC), R1
; @label(PC)
MOV @OFF(PC), R2
