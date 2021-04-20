#include <string.h>

#include "lc3_reg.h"

char const * const REG_R0_STR = "r0";
char const * const REG_R1_STR = "r1";
char const * const REG_R2_STR = "r2";
char const * const REG_R3_STR = "r3";
char const * const REG_R4_STR = "r4";
char const * const REG_R5_STR = "r5";
char const * const REG_R6_STR = "r6";
char const * const REG_R7_STR = "r7";
char const * const REG_PC_STR = "pc";
char const * const REG_PSR_STR = "psr";
char const * const REG_USP_STR = "usp";
char const * const REG_SSP_STR = "ssp";

static const struct {
    char const * const str;
    enum lc3_reg reg;
} reg_str_conversion[] = {
    {REG_R0_STR, REG_R0},
    {REG_R1_STR, REG_R1},
    {REG_R2_STR, REG_R2},
    {REG_R3_STR, REG_R3},
    {REG_R4_STR, REG_R4},
    {REG_R5_STR, REG_R5},
    {REG_R6_STR, REG_R6},
    {REG_R7_STR, REG_R7},
    {REG_PC_STR, REG_PC},
    {REG_PSR_STR, REG_PSR},
    {REG_USP_STR, REG_USP},
    {REG_SSP_STR, REG_SSP},
};

/* string must be lowercase */
/* just a simple linear search because there are only 12 registers and this does not get called very often */
int lc3_reg_str_convert(char *reg_str, enum lc3_reg *reg_result) {
    int i;
    for (i = 0; i < num_registers; ++i) {
        if (strcmp(reg_str_conversion[i].str, reg_str) == 0) {
            *reg_result = reg_str_conversion[i].reg;
            return 1;
        }
    }
    return 0;
}