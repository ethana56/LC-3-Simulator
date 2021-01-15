.ORIG x0180
     ADD R6, R6, #-3
     STR R0, R6, #0
     STR R1, R6, #1
     STR R2, R6, #2
     LDI R0, KBDR
     LD  R2, TILDE
     NOT R2, R2
     ADD R2, R2, #1
     ADD R2, R2, R0
     BRz STOP
LOOP:
     LDI R1, DSR
     BRz LOOP
     STI R0, DDR
     LDR R1, R6, #1
     LDR R0, R6, #0
     ADD R6, R6, #3
     RTI
STOP
   HALT
      
KBDR .FILL xFE02
DSR .FILL  xFE04
DDR .FILL  xFE06
TILDE .FILL #126
.END
