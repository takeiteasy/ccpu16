/* cpu.c -- https://github.com/takeiteasy/mir2
 
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
#include <string.h>

enum {
    A = 0x00, B, C,
    X, Y, Z,
    I, J,
    PC, SP, EX, IA
};

enum {
    SPC = 0x00,
    SET,
    ADD, SUB, MUL, MLI, DIV, DVI, MOD, MDI,
    AND, BOR, XOR, SHR, ASR, SHL,
    IFB, IFC, IFE, IFN, IFG, IFA, IFL, IFU,
    ADX = 0x1A, SBX,
    STI = 0x1E, STD
};

static const uint16_t basic_clocks[0x20] = {
    0, 1, 2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 3, 3, 0, 0, 2, 2
};

enum {
    RES = 0x00,
    JSR,
    INT = 0x08, IAG, IAS, RFI, IAQ,
    HWN = 0x10, HWQ, HWI
};

static const uint16_t spc_clocks[0x20] = {
    0, 3, 0, 0, 0, 0, 0, 0, 4, 1, 1, 3, 2, 0, 0, 0,
    2, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void tick(struct cpu_t *cpu, int ticks) {
    cpu->cycles += ticks;
}

static uint16_t* next_word(struct cpu_t *cpu) {
    tick(cpu, 1);
    return &cpu->memory[cpu->reg[PC]++];
}

static void inc(struct cpu_t *cpu, uint16_t v) {
    switch (v) {
        case IFB ... IFU:
        case ADX:
        case STI:
        case STD:
            cpu->reg[PC]++;
            break;
    }
}

static void skip(struct cpu_t *cpu) {
    tick(cpu, 1);
    uint16_t word = *next_word(cpu);
    uint16_t o = word & 0x1F;
    uint16_t a = (word >> 5) & 0x1F;
    uint16_t b = (word >> 10) & 0x3F;
    inc(cpu, a);
    inc(cpu, b);
    if (o >= IFB && o <= IFU)
        skip(cpu);
}

static uint16_t* lvalue(struct cpu_t *cpu, uint16_t v) {
    if (v < 0x08)
        return &cpu->reg[v];
    else if (v < 0x10)
        return &cpu->memory[cpu->reg[v - 0x08]];
    else if (v < 0x18)
        return &cpu->memory[cpu->reg[v - 0x10] + *next_word(cpu)];
    else
        switch (v) {
            case 0x18: return &cpu->memory[--cpu->reg[SP]];
            case 0x19: return &cpu->memory[cpu->reg[SP]];
            case 0x1A: return &cpu->memory[cpu->reg[SP] + *next_word(cpu)];
            case 0x1B: return &cpu->reg[SP];
            case 0x1C: return &cpu->reg[PC];
            case 0x1D: return &cpu->reg[EX];
            case 0x1E: return &cpu->memory[*next_word(cpu)];
            case 0x1F: return next_word(cpu);
            default:   return NULL;
        }
}

static uint16_t rvalue(struct cpu_t *cpu, uint16_t v) {
    if (v == 0x18)
        return cpu->memory[cpu->reg[SP]++];
    else if (v < 0x20)
        return *lvalue(cpu, v);
    else
        return v - 0x21;
}

static void basic(struct cpu_t *cpu, uint16_t word) {
    uint16_t  a = rvalue(cpu, (word >> 10) & 0x3F);
    uint16_t *b = lvalue(cpu, (word >> 5)  & 0x1F);
    uint16_t  o = word & 0x1F;
    tick(cpu, basic_clocks[o] - 1);
    uint16_t ex = 0;
    switch (o) {
        case SET: *b = a; break;
        case ADD: cpu->reg[EX] = (((uint32_t)*b + (uint32_t)a) >> 16) & 0xFFFF; *b += a; break;
        case SUB: cpu->reg[EX] =  (((int32_t)*b -  (int32_t)a) >> 16) & 0xFFFF; *b -= a; break;
        case MUL: cpu->reg[EX] = (((uint32_t)*b * (uint32_t)a) >> 16) & 0xFFFF; *b *= a; break;
        case MLI: cpu->reg[EX] =  (((int32_t)*b *  (int32_t)a) >> 16) & 0xFFFF; *(int16_t*)b *= (int16_t)a; break;
        case DIV:
            if (a == 0) {
                cpu->reg[EX] = 0;
                *b = 0;
            } else {
                cpu->reg[EX] = (((uint32_t)*b << 16) / (uint32_t)a) & 0xFFFF;
                *b /= a;
            }
            break;
        case DVI:
            if (a == 0) {
                cpu->reg[EX] = 0;
                *b = 0;
            } else {
                cpu->reg[EX] = (((int32_t)*b << 16) / (int32_t)a) & 0xFFFF;
                *(int16_t*)b /= (int16_t)a;
            }
            break;
        case MOD: if (a == 0) *b = 0; else *b %= a; break;
        case MDI: if (a == 0) *b = 0; else *(int16_t *)b %= (int16_t)a; break;
        case AND: *b &= a; break;
        case BOR: *b |= a; break;
        case XOR: *b ^= a; break;
        case SHR: cpu->reg[EX] = (((uint32_t)*b << 16) >> a)  & 0xFFFF; *b >>= a; break;
        case ASR: cpu->reg[EX] = (((uint32_t)*b << 16) >> a)  & 0xFFFF; *(int16_t*)b >>= a; break;
        case SHL: cpu->reg[EX] = (((uint32_t)*b << a)  >> 16) & 0xFFFF; *b <<= a; break;
        case IFB: if ((*b & a) == 0) skip(cpu); break;
        case IFC: if ((*b & a) != 0) skip(cpu); break;
        case IFE: if  (*b != a) skip(cpu); break;
        case IFN: if  (*b == a) skip(cpu); break;
        case IFG: if  (*b <= a) skip(cpu); break;
        case IFL: if  (*b >= a) skip(cpu); break;
        case IFA: if (*(int16_t*)b <= (int16_t)a) skip(cpu); break;
        case IFU: if (*(int16_t*)b >= (int16_t)a) skip(cpu); break;
        case ADX:
            ex = (((uint32_t)*b + (uint32_t)a + (uint32_t)cpu->reg[EX]) >> 16) & 0xFFFF;
            *b += (a + cpu->reg[EX]);
            cpu->reg[EX] = ex;
            break;
        case SBX:
            ex = (((int32_t)*b - ((int32_t)a + (int32_t)cpu->reg[EX])) >> 16) & 0xFFFF;
            *b -= (a + cpu->reg[EX]);
            cpu->reg[EX] = ex;
            break;
        case STI: *b = a; cpu->reg[I] += 1; cpu->reg[J] += 1; break;
        case STD: *b = a; cpu->reg[I] -= 1; cpu->reg[J] -= 1; break;
        default:
            cpu->state = CPU_HALT;
    }
}

static void special(struct cpu_t *cpu, uint16_t word) {
    uint16_t o = (word >> 5) & 0x1F;
    if (o == RES) {
        cpu->state = CPU_HALT;
        return;
    }
    tick(cpu, spc_clocks[o]);

    uint16_t a, *b;
    if (((word >> 10) & 0x3F) < 0x20) {
        b = lvalue(cpu, (word >> 10) & 0x1F);
        a = *b;
    } else {
        b = NULL;
        a = rvalue(cpu, (word >> 10) & 0x3F);
    }

    switch (o) {
        case JSR: cpu->memory[--cpu->reg[SP]] = cpu->reg[PC]; cpu->reg[PC] = a; break;
        case INT: cpu_interrupt(cpu, a); break;
        case IAG: if (b) *b = cpu->reg[IA]; break;
        case IAS: cpu->reg[IA] = a; break;
        case RFI:
            cpu->iaq_enabled = 1;
            cpu->reg[A]  = cpu->memory[cpu->reg[SP]++];
            cpu->reg[PC] = cpu->memory[cpu->reg[SP]++];
            break;
        case IAQ: cpu->iaq_enabled = !a; break;
        case HWN: if (b) *b = cpu->hardware_count; break;
        case HWQ:
            if (a < cpu->hardware_count && cpu->hardware[a].enabled) {
                cpu->reg[A] = cpu->hardware[a].id & 0xFFFF;
                cpu->reg[B] = (cpu->hardware[a].id >> 16) & 0xFFFF;
                cpu->reg[C] = cpu->hardware[a].version & 0xFFFF;
                cpu->reg[X] = cpu->hardware[a].manufacturer & 0xFFFF;
                cpu->reg[Y] = (cpu->hardware[a].manufacturer >> 16) & 0xFFFF;
            } else {
                cpu->reg[A] = 0;
                cpu->reg[B] = 0;
                cpu->reg[C] = 0;
                cpu->reg[X] = 0;
                cpu->reg[Y] = 0;
            }
            break;
        case HWI:
            if (a < cpu->hardware_count &&
                cpu->hardware[a].enabled &&
                cpu->hardware[a].interrupt)
                cpu->hardware[a].interrupt(&cpu->hardware[a]);
            break;
        default:
            cpu->state = CPU_HALT;
    }
}

void cpu_step(struct cpu_t *cpu) {
    if (cpu->state == CPU_HALT ||
        cpu->state == CPU_ON_FIRE)
        return;
    
    if (!cpu->iaq_enabled && cpu->iaq_index)
        cpu_interrupt(cpu, cpu->iaq[--cpu->iaq_index]);
    
    for (int i = 0; i < cpu->hardware_count; i++) {
        struct hardware_t *hw = &cpu->hardware[i];
        if (hw->enabled && hw->tick)
            hw->tick(hw);
    }
    
    uint16_t word = *next_word(cpu);
    if ((word & 0x1F) == SPC)
        special(cpu, word);
    else
        basic(cpu, word);
}

void cpu_interrupt(struct cpu_t *cpu, uint16_t message) {
    if (!cpu->reg[IA])
        return;
    if (!cpu->iaq_enabled) {
        cpu->iaq_enabled = 1;
        cpu->memory[--cpu->reg[SP]] = cpu->reg[PC];
        cpu->memory[--cpu->reg[SP]] = cpu->reg[A];
        cpu->reg[PC] = cpu->reg[IA];
        cpu->reg[A]  = message;
    } else {
        if (cpu->iaq_index >= 256)
            cpu->state = CPU_ON_FIRE;
        else
            cpu->iaq[cpu->iaq_index++] = message;
    }
}

int cpu_attach_hardware(struct cpu_t *cpu, int(*init_cb)(struct hardware_t*)) {
    if (cpu->hardware_count >= 0xFFFF)
        return 0;
    struct hardware_t *hw = &cpu->hardware[cpu->hardware_count++];
    memset(hw, 0, sizeof(struct hardware_t));
    hw->enabled = 1;
    hw->cpu = cpu;
    int result = init_cb ? init_cb(hw) : 1;
    if (hw->init)
        hw->init(hw);
    return result;
}
