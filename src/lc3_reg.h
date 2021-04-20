#ifndef LC3_REG_H
#define LC3_REG_H

enum lc3_reg {REG_R0, REG_R1, REG_R2, REG_R3, REG_R4, REG_R5, 
    REG_R6, REG_R7, REG_PC, REG_PSR, REG_USP, REG_SSP, num_registers};

int lc3_reg_str_convert(char *, enum lc3_reg *);

#endif