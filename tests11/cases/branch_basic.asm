ORG 0200
start:
BR forward
NOP
forward:
BR start
BEQ start
BNE forward
