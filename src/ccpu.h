/* ccpu.h -- https://github.com/takeiteasy/mir2
 
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

#ifndef CCPU_H
#define CCPU_H
#include <stdint.h>

struct cpu_t;

struct hardware_t {
    uint32_t id;
    uint16_t version;
    uint32_t manufacturer;
    uint8_t  enabled;
    
    struct cpu_t *cpu;
    void *data;
    
    void(*init)(struct hardware_t*);
    void(*tick)(struct hardware_t*);
    void(*interrupt)(struct hardware_t*);
    void(*deinit)(struct hardware_t*);
};

enum cpu_state {
    CPU_IDLE = 0,
    CPU_OK,
    CPU_HALT,
    CPU_ON_FIRE
};

struct cpu_t {
    uint16_t reg[12];
    enum cpu_state state;
    uint16_t iaq_enabled;
    uint16_t iaq_index;
    uint16_t iaq[256];
    uint16_t memory[0x10000];
    struct hardware_t hardware[0xFFFF];
    uint16_t hardware_count;
    uint64_t cycles;
};

void cpu_step(struct cpu_t *cpu);
void cpu_interrupt(struct cpu_t *cpu, uint16_t message);
int cpu_attach_hardware(struct cpu_t *cpu, int(*init_cb)(struct hardware_t*));

// int assemble(const char *src, uint16_t dst[0x10000]);
int disassemble(uint16_t *cursor, char dst[32]);

#endif // CCPU_H
