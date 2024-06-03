//
//  test.c
//  ccpu
//
//  Created by George Watson on 20/05/2024.
//

#include "ccpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int assemble(const char *src, uint16_t dst[0x10000]);

static void load(struct cpu_t *cpu, const char *path) {
    FILE *fh = fopen(path, "rb");
    if (!fh)
        abort();
    fseek(fh, 0, SEEK_END);
    size_t sz = ftell(fh);
    if (sz >= 0x10000)
        abort();
    fseek(fh, 0, SEEK_SET);
    fread(cpu->memory, sz, 1, fh);
    fclose(fh);
}

static void compile(struct cpu_t *cpu, const char *path) {
    FILE *fh = fopen(path, "rb");
    if (!fh)
        abort();
    fseek(fh, 0, SEEK_END);
    size_t sz = ftell(fh);
    if (sz >= 0x10000)
        abort();
    fseek(fh, 0, SEEK_SET);
    char *src = malloc(sz * sizeof(char));
    fread(src, sz, 1, fh);
    assemble(src, cpu->memory);
    fclose(fh);
    free(src);
}

int main(int argc, const char *argv[]) {
    struct cpu_t cpu;
    memset(&cpu, 0, sizeof(struct cpu_t));
//    load(&cpu, "tests/sample.bin");
    compile(&cpu, "tests/sample.s");
    
    fprintf(stdout,
        "PC   SP   EX   IA   A    B    C    X    Y    Z    I    J    Instruction\n"
        "---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- -----------\n");
    while (cpu.state != CPU_HALT &&
         cpu.state != CPU_ON_FIRE) {
        char buf[32];
        disassemble(&cpu.memory[cpu.reg[8]], buf);
        fprintf(stdout,
                "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %s\n",
                cpu.reg[8], cpu.reg[9], cpu.reg[10], cpu.reg[11],
                cpu.reg[0], cpu.reg[1], cpu.reg[2], cpu.reg[3],
                cpu.reg[4], cpu.reg[5], cpu.reg[6], cpu.reg[7],
                buf);
        cpu_step(&cpu);
    }
    return 0;
}
