/* assembler.c -- https://github.com/takeiteasy/mir2
 
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
#include <stdlib.h>
#include <string.h>

typedef struct label_t {
    const char *name;
    uint16_t pc;
    struct label_t *next;
} label_t;

typedef struct {
    uint16_t *dst, buf[0x10000];
    const char *src;
    size_t src_length;
    int col, row, pc;
    label_t *labels;
} assembler_t;

typedef struct {
    const char *name;
    uint16_t value;
} opcode_t;

static opcode_t basic_ops[] = {
    {"SET", 0x01},
    {"ADD", 0x02},
    {"SUB", 0x03},
    {"MUL", 0x04},
    {"MLI", 0x05},
    {"DIV", 0x06},
    {"DVI", 0x07},
    {"MOD", 0x08},
    {"MDI", 0x09},
    {"AND", 0x0a},
    {"BOR", 0x0b},
    {"XOR", 0x0c},
    {"SHR", 0x0d},
    {"ASR", 0x0e},
    {"SHL", 0x0f},
    {"IFB", 0x10},
    {"IFC", 0x11},
    {"IFE", 0x12},
    {"IFN", 0x13},
    {"IFG", 0x14},
    {"IFA", 0x15},
    {"IFL", 0x16},
    {"IFU", 0x17},
    {"ADX", 0x1a},
    {"SBX", 0x1b},
    {"STI", 0x1e},
    {"STD", 0x1f}
};
#define BASIC_OP_LEN (sizeof(basic_ops) / sizeof(basic_ops[0]))

static opcode_t special_ops[] = {
    {"JSR", 0x01},
    {"INT", 0x08},
    {"IAG", 0x09},
    {"IAS", 0x0a},
    {"RFI", 0x0b},
    {"IAQ", 0x0c},
    {"HWN", 0x10},
    {"HWQ", 0x11},
    {"HWI", 0x12}
};
#define SPECIAL_OP_LEN (sizeof(special_ops) / sizeof(special_ops[0]))

static void to_upper(char *word, size_t length) {
    for (char *cursor = word; *cursor; cursor++)
        if (*cursor >= 'a' && *cursor <= 'z')
            *cursor -= 32;
}

static char* read_word(char *cursor, size_t *length) {
    static char buffer[128];
    memset(buffer, 0, 128);
    buffer[0] = '\0';
    int _length = 0, off = 0;
    char *word = cursor;
    while (*word++ != ' ')
        _length++;
    if (!_length)
        return NULL; // Unexpected error
    if (_length > 128)
        return NULL; // Overflow error, word too long
    memcpy(buffer, cursor + off, _length * sizeof(char));
    to_upper(buffer, _length);
    if (length)
        *length = _length;
    return buffer;
}

static int lookup(char *word, size_t length, int *index) {
    int i, result = 0;
    for (i = 0; i < BASIC_OP_LEN; i++)
        if (!strncmp(word, basic_ops[i].name, length)) {
            result = 1;
            goto END; // Name matches basic op
        }
    for (i = 0; i < SPECIAL_OP_LEN; i++)
        if (!strncmp(word, special_ops[i].name, length)) {
            result = 2;
            goto END; // Name matches special op
        }
    i = -1;
END:
    if (index)
        *index = i;
    return result;
}

static int process_label(assembler_t *ass, char *word, size_t length) {
    char *name = word + 1;
    if (*name == '\0' || !name || length <= 1)
        return 0;
    for (char *cursor = name; *cursor; cursor++) {
        char c = *cursor;
        switch (c) {
            case 'A' ... 'Z':
            case '_':
                break;
            default:
                return 0; // Invalid character in label
        }
    }
    
    int type = lookup(name, length-1, NULL);
    if (type > 0)
        return 0; // Name is an op
    
    label_t *cursor = ass->labels;
    if (cursor)
        for (;;) {
            if (!strncmp(cursor->name, name, length-1))
                return 0; // Already defined
            if (!cursor->next)
                break;
            else
                cursor = cursor->next;
        }
    
    label_t *new = malloc(sizeof(label_t));
    new->name = strdup(name);
    new->pc   = ass->pc;
    new->next = NULL;
    if (cursor)
        cursor->next = new;
    else
        ass->labels = new;
    return 1;
}

#define INCR(N)      \
do {                 \
    ass->col += (N); \
    cursor += (N);   \
} while(0)

static char* skip_whitespace(assembler_t *ass, char *cursor) {
    int n = 0;
    char *skip = cursor;
    while (*skip == ' ' && *skip++ != '\0')
        n++;
    ass->col += n;
    return skip;
}

enum {
    tERROR, tOBRKT, tREG, tRADDR, tLITERAL, tINTEGER
};

static uint16_t process_operand(assembler_t *ass, char *word, size_t length, int *type) {
    switch (*word) {
        case '[':
            if (*type == tOBRKT)
                *type = tERROR;
            else
                *type = tOBRKT;
            char *w = skip_whitespace(ass, word + 1);
            ass->col++;
            return process_operand(ass, w, length - 1, type);
        case '0' ... '9':;
            if (*word == '0' &&
                (length == 6 || length == 4) &&
                (word[1] == 'X' || word[1] == 'x')) {
                for (const char *tmp = word + 2; *tmp; tmp++)
                    switch (*tmp) {
                        case '0' ... '9':
                        case 'a' ... 'f':
                        case 'A' ... 'F':
                            break;
                        default:
                            return -1; // Invalid number
                    }
                *type = tLITERAL;
                return strtol(word, NULL, 16);
            }
            const char *tmp_number = word;
            for (; *tmp_number; tmp_number++)
                if (*tmp_number < '0' || *tmp_number > '9')
                    return -1; // Invalid number
            *type = tINTEGER;
            return atoi(tmp_number);
        case 'A' ... 'Z':
        case 'a' ... 'z':
        case '_':;
            if (length == 1) {
                switch (*word) {
                    case 'A' ... 'C':
                    case 'X' ... 'Z':
                    case 'I':
                    case 'J':;
                        static const char *regs = "ABCXYZIJ";
                        for (int i = 0; i < 8; i++)
                            if (regs[i] == *word) {
                                *type = tREG;
                                return i & 7;
                            }
                }
            } else if (length == 2) {
                if (!strncmp(word, "SP", 2))
                    return 0x1B;
                if (!strncmp(word, "PC", 2))
                    return 0x1C;
                if (!strncmp(word, "EX", 2))
                    return 0x1D;
            }
            const char *tmp_string = word;
            for (; *tmp_string; tmp_string++)
                switch (*tmp_string) {
                    case 'A' ... 'Z':
                    case 'a' ... 'z':
                    case '_':
                        break;
                    default:
                        return -1; // Invalid string
                }
            break;
    }
    return 0;
}

static uint16_t read_first_operand(assembler_t *ass, char *cursor) {
    size_t length;
    cursor = skip_whitespace(ass, cursor);
    char *word = read_word(cursor, &length);
    if (!word || !length)
        return -1;
    if (word[length - 1] == ',')
        word[--length] = '\0';
    INCR(length);
    int type;
    return process_operand(ass, word, length, &type);
}

static int process_bop(assembler_t *ass, char *cursor, int idx) {
    opcode_t *op = &basic_ops[idx];
    
    int prev_col = ass->col;
    uint16_t b = read_first_operand(ass, cursor);
    if (b < 0)
        return 0; // Invalid operand
    cursor += ass->col - prev_col;
    
    cursor = skip_whitespace(ass, cursor);
    if (*cursor != ',')
        return 0; // Expected a comma
    else
        INCR(1);
    
    cursor = skip_whitespace(ass, cursor);
    size_t length;
    char *second = read_word(cursor, &length);
    int type;
    uint16_t a = process_operand(ass, second, length, &type);
    
    return 1;
}

static void process_spc(assembler_t *ass, int idx) {
    
}

static int process_line(assembler_t *ass, char *line) {
    char *cursor = line;
    while (*cursor)
        switch (*cursor) {
            case ' ':
            case '\r':
                INCR(1);
                break;
            case '\n':
            case ';':
                return 1; // skip line
            default:;
                size_t length;
                char *word = read_word(cursor, &length);
                if (!word || !length)
                    return 0;
                else
                    INCR(length);
                switch (*word) {
                    case 'A' ... 'Z':
                    case '0' ... '9':
                    case '_':;
                        int idx = -1;
                        switch (lookup(word, length, &idx)) {
                            default:
                            case 0:
                                return 0; // Expected op
                            case 1:
                                if (!process_bop(ass, cursor, idx))
                                    return 0;
                                break;
                            case 2:
                                break;
                        }
                        break;
                    case ':':
                        if (!process_label(ass, word, length))
                            return 0; // Invalid label or attempted redefine
                        else
                            INCR(length);
                        break;
                    default:
                        return 0; // Expected label or op
                }
        }
    return 1;
}

static int process(assembler_t *ass) {
    const char *line = ass->src;
    int result = 1;
    while (line) {
        char buffer[128];
        char *next_line = strchr(line, '\n');
        long len = next_line ? (next_line-line) : strlen(line);
        if (len > 128)
            len = 128;
        memcpy(buffer, line, len * sizeof(char));
        if (len < 128)
            buffer[len] = '\0';
        if ((result = process_line(ass, buffer)) != 1)
            return result;
        line = next_line ? next_line + 1 : NULL;
        ass->row++;
        ass->col = 0;
    }
    return result;
}

int assemble(const char *src, uint16_t dst[0x10000]) {
    assembler_t ass;
    memset(&ass, 0, sizeof(assembler_t));
    ass.dst = dst;
    ass.src = src;
    ass.src_length = strlen(src);
    int result = process(&ass);
    // fix labels here ...
    label_t *head = ass.labels;
    while (head) {
        label_t *next = head->next;
        free((void*)next->name);
        free(next);
        head = next;
    }
    return result;
}
