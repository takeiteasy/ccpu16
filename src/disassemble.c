/* disassemble.c -- https://github.com/takeiteasy/mir2
 
 The MIT License (MIT)
 
 Copyright (c) 2024 George Watson
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "ccpu.h"
#include <stdio.h>
#include <string.h>

static char lval[16], rval[16];

const char *opcode_table[0x20] = {
    "",
    "SET", "ADD", "SUB", "MUL", "MLI", "DIV", "DVI", "MOD", "MDI",
    "AND", "BOR", "XOR", "SHR", "ASR", "SHL",
    "IFB", "IFC", "IFE", "IFN", "IFG", "IFA", "IFL", "IFU",
    "", "",
    "ADX", "SBX",
    "", "",
    "STI", "STD"
};

const char *spc_opcode_table[0x20] = {
    "",
    "JSR",
    "", "", "", "", "", "",
    "INT", "IAG", "IAS", "RFI", "IAQ",
    "", "", "",
    "HWN", "HWQ", "HWI",
    "", "", "", "", "", "", "", "", "", "", "", "", ""
};

const char *gpr_table[0x08] = {
    "A", "B", "C", "X", "Y", "Z", "I", "J"
};

static int disasm_lval(uint16_t val, uint16_t next) {
    int result = 0;
    if (val < 0x08)
        sprintf(lval, "%s", gpr_table[val]);
    else if (val < 0x10)
        sprintf(lval, "[%s]", gpr_table[val - 0x08]);
    else if (val < 0x18) {
        result = 1;
        sprintf(lval, "[0x%.4x+%s]", next, gpr_table[val - 0x10]);
    } else {
        switch (val) {
        case 0x18:
            sprintf(lval, "PUSH");
            break;
        case 0x19:
            sprintf(lval, "PEEK");
            break;
        case 0x1a:
            result = 1;
            sprintf(lval, "[SP+0x%.4x]", next);
            break;
        case 0x1b:
            sprintf(lval, "SP");
            break;
        case 0x1c:
            sprintf(lval, "PC");
            break;
        case 0x1d:
            sprintf(lval, "EX");
            break;
        case 0x1e:
            result = 1;
            sprintf(lval, "[0x%.4x]", next);
            break;
        case 0x1f:
            result = 1;
            sprintf(lval, "0x%.4x", next);
            break;
        }
    }
    return result;
}

static int disasm_rval(uint16_t val, uint16_t next) {
    int rv = 0;
    char temp[16];
    if (val >= 0x20)
        sprintf(rval, "0x%.4x", (int16_t)val - 0x21);
    else if (val == 0x18)
        sprintf(rval, "POP");
    else {
        strncpy(temp, lval, 16);
        rv = disasm_lval(val, next);
        strncpy(rval, lval, 16);
        strncpy(lval, temp, 16);
    }
    return rv;
}

int disassemble(uint16_t *memory, char *dst) {
    uint16_t word = memory[0];
    int words = 1;
    if (word & 0x1F) {
        words += disasm_rval((word >> 10) & 0x3F, memory[words]);
        words += disasm_lval((word >> 5)  & 0x1F, memory[words]);
        sprintf(dst, "%s %s, %s", opcode_table[word & 0x1F], lval, rval);
    } else if ((word >> 5) & 0x1F) {
        words += disasm_rval((word >> 10) & 0x3F, memory[words]);
        sprintf(dst, "%s %s", spc_opcode_table[(word >> 5) & 0x1F], rval);
    } else
        sprintf(dst, "DAT 0x%.4x", word);
    return words;
}
