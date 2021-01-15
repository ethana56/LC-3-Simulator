.ORIG x0000

.BLKW 32

GETC_ADDR .FILL TRAP_GETC
OUT_ADDR  .FILL TRAP_OUT
PUTS_ADDR .FILL TRAP_PUTS
.BLKW 2
HALT_ADDR .FILL TRAP_HALT

REST .BLKW 485

TRAP_GETC     LDI R0, KBSR
              BRzp TRAP_GETC
              LDI R0, KBDR
              RET

TRAP_OUT      ST R1, SAVE_R1
OUT_LOOP      LDI R1, DSR
              BRzp TRAP_OUT
              STI R0, DDR
              LD R1, SAVE_R1
              RET

TRAP_PUTS     ST R1, SAVE_R1
              ST R2, SAVE_R2
PUTS_LOOP     LDR R1, R0, 0
              BRz PUTS_END
PUTS_OUT_LOOP LDI R2, DSR
              BRzp PUTS_OUT_LOOP
              STI R1, DDR
              ADD R0, R0, 1
              BR PUTS_LOOP
PUTS_END      LD R1, SAVE_R1
              LD R2, SAVE_R2
              RET

                    


TRAP_HALT     AND R0, R0, #0
              STI R0, MCR
              RET

KBSR .FILL xFE00
KBDR .FILL xFE02
DSR  .FILL xFE04
DDR  .FILL xFE06
MCR  .FILL xFFFE
SAVE_R1 .FILL 0
SAVE_R2 .FILL 0
.END
