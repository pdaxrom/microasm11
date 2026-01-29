ORG 0
start:
DS 250 ; 500 octal bytes = 320 dec? No 250 bytes? words? 
; 256 bytes = 128 words. range limit.
; DS 260 ensures > 256 bytes.
DS 300 
target:
BR start
