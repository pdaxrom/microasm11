ORG 0200
sub:
RTS R5

start:
JSR R5, sub
JMP (R5)
JMP @#sub
