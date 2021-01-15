.ORIG x0180
     ADD R6, R6, -2
     STR R0, R6, #0
     STR R1, R6, #1
     LDI R0, KBDR
LOOP:
     LDI R1, DSR
     BRz LOOP
     STI R0, DDR
     LDR R1, R6, #1
     LDR R0, R6, #0
     ADD R6, R6, #2
     RTI
     
LDI 
HALT
KBDR .FILL xFE02
DSR .FILL  xFE04
DDR .FILL  xFE06
.END