/*
 * Assembler for PDP/11
 * based on microasm for microcpu https://github.com/pdaxrom/microcpu
 * (c) sashz by pdaXrom.org, 2026
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

enum {
    NO_ERROR = 0,
    NO_MEMORY_FOR_LABEL,
    CANNOT_RESOLVE_REF,
    NO_MEMORY_FOR_MACRO,
    NO_MEMORY_FOR_PROC,
    INVALID_NUMBER,
    INVALID_HEX_NUMBER,
    INVALID_DECIMAL_NUMBER,
    INVALID_OCTAL_NUMBER,
    INVALID_BINARY_NUMBER,
    MISSED_BRACKET,
    EXPECTED_CLOSE_QUOTE,
    MISSED_OPCODE_PARAM_1,
    LONG_RELATED_OFFSET,
    MISSED_OPCODE_ARG_1,
    EXPECTED_ARG_2,
    MISSED_REGISTER_ARG_2,
    EXPECTED_ARG_3,
    CONSTANT_VALUE_TOO_BIG,
    OUTPUT_BUFFER_OVERFLOW,
    MISSED_NAME_FOR_EQU,
    MISSED_NAME_FOR_PROC,
    NESTED_PROC_UNSUPPORTED,
    ONLY_INSIDE_PROC,
    LABEL_ALREADY_DEFINED,
    MACRO_ALREADY_DEFINED,
    PROC_ALREADY_DEFINED,
    EXTRA_SYMBOLS,
    SYNTAX_ERROR,
    CANNOT_OPEN_FILE,
    UNSUPPORTED_INSTRUCTION,
};

enum {
    op_none = 0,
    op_single,
    op_double,
    op_branch,
    op_jmp,
    op_jsr,
    op_rts,
    op_sob,
    op_mark,
    op_eis,
    op_xor,
    op_trap,
    op_emt,
    op_spl,
    op_ccode,
    op_fis,

    pseudo_db,
    pseudo_dw,
    pseudo_ds,
    pseudo_dsw,
    pseudo_align,
    pseudo_macro,
    pseudo_equ,
    pseudo_proc,
    pseudo_org,
    pseudo_include,
    pseudo_chksum,
    pseudo_cpu,
};

typedef struct {
    const char *name;
    int type;
    unsigned short base;
    int allow_byte;
    unsigned int cpu_mask;
} OpCode;

enum {
    CPU_DEFAULT = 1 << 0,
    CPU_DCJ11  = 1 << 1,
    CPU_VM1    = 1 << 2,
    CPU_VM1G   = 1 << 3,
    CPU_VM2    = 1 << 4,
};

#define CPU_ALL (CPU_DEFAULT | CPU_DCJ11 | CPU_VM1 | CPU_VM1G | CPU_VM2)

static unsigned int current_cpu = CPU_DEFAULT;

static int set_cpu_by_name(const char *name)
{
    if (!name || !*name) {
        return 0;
    }
    if (!strcasecmp(name, "default")) {
        current_cpu = CPU_DEFAULT;
        return 1;
    }
    if (!strcasecmp(name, "dcj-11") || !strcasecmp(name, "dcj11")) {
        current_cpu = CPU_DCJ11;
        return 1;
    }
    if (!strcasecmp(name, "vm1")) {
        current_cpu = CPU_VM1;
        return 1;
    }
    if (!strcasecmp(name, "vm1g")) {
        current_cpu = CPU_VM1G;
        return 1;
    }
    if (!strcasecmp(name, "vm2")) {
        current_cpu = CPU_VM2;
        return 1;
    }
    return 0;
}

static OpCode opcode_table[] = {
    /* double operand */
    { "mov",  op_double, 0010000, 1, CPU_ALL },
    { "cmp",  op_double, 0020000, 1, CPU_ALL },
    { "bit",  op_double, 0030000, 1, CPU_ALL },
    { "bic",  op_double, 0040000, 1, CPU_ALL },
    { "bis",  op_double, 0050000, 1, CPU_ALL },
    { "add",  op_double, 0060000, 0, CPU_ALL },
    { "sub",  op_double, 0160000, 0, CPU_ALL },

    /* single operand */
    { "clr",  op_single, 005000, 1, CPU_ALL },
    { "com",  op_single, 005100, 1, CPU_ALL },
    { "inc",  op_single, 005200, 1, CPU_ALL },
    { "dec",  op_single, 005300, 1, CPU_ALL },
    { "neg",  op_single, 005400, 1, CPU_ALL },
    { "adc",  op_single, 005500, 1, CPU_ALL },
    { "sbc",  op_single, 005600, 1, CPU_ALL },
    { "tst",  op_single, 005700, 1, CPU_ALL },
    { "ror",  op_single, 006000, 1, CPU_ALL },
    { "rol",  op_single, 006100, 1, CPU_ALL },
    { "asr",  op_single, 006200, 1, CPU_ALL },
    { "asl",  op_single, 006300, 1, CPU_ALL },
    { "swab", op_single, 000300, 0, CPU_ALL },
    { "sxt",  op_single, 006700, 0, CPU_ALL },
    { "csm",  op_single, 007000, 0, CPU_DEFAULT | CPU_DCJ11 },
    { "tstset", op_single, 007200, 0, CPU_DEFAULT | CPU_DCJ11 },
    { "wrtlck", op_single, 007300, 0, CPU_DEFAULT | CPU_DCJ11 },

    /* branches */
    { "br",   op_branch, 0000400, 0, CPU_ALL },
    { "bne",  op_branch, 0001000, 0, CPU_ALL },
    { "beq",  op_branch, 0001400, 0, CPU_ALL },
    { "bpl",  op_branch, 0100000, 0, CPU_ALL },
    { "bmi",  op_branch, 0100400, 0, CPU_ALL },
    { "bvc",  op_branch, 0102000, 0, CPU_ALL },
    { "bvs",  op_branch, 0102400, 0, CPU_ALL },
    { "bcc",  op_branch, 0103000, 0, CPU_ALL },
    { "bcs",  op_branch, 0103400, 0, CPU_ALL },
    { "bge",  op_branch, 0002000, 0, CPU_ALL },
    { "blt",  op_branch, 0002400, 0, CPU_ALL },
    { "bgt",  op_branch, 0003000, 0, CPU_ALL },
    { "ble",  op_branch, 0003400, 0, CPU_ALL },
    { "bhi",  op_branch, 0101000, 0, CPU_ALL },
    { "blos", op_branch, 0101400, 0, CPU_ALL },

    /* program control */
    { "jmp",  op_jmp,    0000100, 0, CPU_ALL },
    { "jsr",  op_jsr,    0004000, 0, CPU_ALL },
    { "rts",  op_rts,    0000200, 0, CPU_ALL },
    { "sob",  op_sob,    0077000, 0, CPU_ALL },
    { "mark", op_mark,   0006400, 0, CPU_ALL },

    /* EIS */
    { "mul",  op_eis,    0070000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM1G | CPU_VM2 },
    { "div",  op_eis,    0071000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM1G | CPU_VM2 },
    { "ash",  op_eis,    0072000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM1G | CPU_VM2 },
    { "ashc", op_eis,    0073000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM1G | CPU_VM2 },
    { "xor",  op_xor,    0074000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM1 | CPU_VM1G | CPU_VM2 },

    /* FIS (KE11-F) */
    { "fadd", op_fis,    075000, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "fsub", op_fis,    075010, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "fmul", op_fis,    075020, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "fdiv", op_fis,    075030, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "cfcc", op_none,   075004, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },

    /* system & trap */
    { "halt", op_none,   0000000, 0, CPU_ALL },
    { "wait", op_none,   0000001, 0, CPU_ALL },
    { "rti",  op_none,   0000002, 0, CPU_ALL },
    { "bpt",  op_none,   0000003, 0, CPU_ALL },
    { "iot",  op_none,   0000004, 0, CPU_ALL },
    { "reset",op_none,   0000005, 0, CPU_ALL },
    { "rtt",  op_none,   0000006, 0, CPU_ALL },
    { "mfpt", op_none,   0000007, 0, CPU_ALL },
    { "trap", op_trap,   0104400, 0, CPU_ALL },
    { "emt",  op_emt,    0104000, 0, CPU_ALL },

    /* VM2 system */
    { "go",   op_none,   0000012, 0, CPU_DEFAULT | CPU_VM2 },
    { "step", op_none,   0000016, 0, CPU_DEFAULT | CPU_VM2 },
    { "rsel", op_none,   0000020, 0, CPU_DEFAULT | CPU_VM2 },
    { "mfus", op_none,   0000021, 0, CPU_DEFAULT | CPU_VM2 },
    { "rcpc", op_none,   0000022, 0, CPU_DEFAULT | CPU_VM2 },
    { "rcps", op_none,   0000024, 0, CPU_DEFAULT | CPU_VM2 },
    { "mtus", op_none,   0000031, 0, CPU_DEFAULT | CPU_VM2 },
    { "wcpc", op_none,   0000032, 0, CPU_DEFAULT | CPU_VM2 },
    { "wcps", op_none,   0000034, 0, CPU_DEFAULT | CPU_VM2 },

    /* memory management */
    { "mfpi", op_single, 0006500, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "mtpi", op_single, 0006600, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "mfpd", op_single, 0106500, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "mtpd", op_single, 0106600, 0, CPU_DEFAULT | CPU_DCJ11 | CPU_VM2 },
    { "mtps", op_single, 0106400, 0, CPU_ALL },
    { "mfps", op_single, 0106700, 0, CPU_ALL },

    /* spl */
    { "spl",  op_spl,    0000230, 0, CPU_ALL },

    /* condition codes */
    { "clc",  op_ccode,  0000241, 0, CPU_ALL },
    { "clv",  op_ccode,  0000242, 0, CPU_ALL },
    { "clz",  op_ccode,  0000244, 0, CPU_ALL },
    { "cln",  op_ccode,  0000250, 0, CPU_ALL },
    { "sec",  op_ccode,  0000261, 0, CPU_ALL },
    { "sev",  op_ccode,  0000262, 0, CPU_ALL },
    { "sez",  op_ccode,  0000264, 0, CPU_ALL },
    { "sen",  op_ccode,  0000270, 0, CPU_ALL },
    { "ccc",  op_ccode,  0000257, 0, CPU_ALL },
    { "scc",  op_ccode,  0000277, 0, CPU_ALL },
    { "nop",  op_ccode,  0000240, 0, CPU_ALL },

    /* pseudo ops */
    { "db", pseudo_db, 0x0, 0, CPU_ALL },
    { "dw", pseudo_dw, 0x0, 0, CPU_ALL },
    { "ds", pseudo_ds, 0x0, 0, CPU_ALL },
    { "dsb", pseudo_ds, 0x0, 0, CPU_ALL },
    { "dsw", pseudo_dsw, 0x0, 0, CPU_ALL },
    { "even", pseudo_align, 0x0, 0, CPU_ALL },
    { "macro", pseudo_macro, 0x0, 0, CPU_ALL },
    { "endm", pseudo_macro, 0x0, 0, CPU_ALL },
    { "equ", pseudo_equ, 0x0, 0, CPU_ALL },
    { "proc", pseudo_proc, 0x0, 0, CPU_ALL },
    { "endp", pseudo_proc, 0x0, 0, CPU_ALL },
    { "global", pseudo_proc, 0x0, 0, CPU_ALL },
    { "org", pseudo_org, 0x0, 0, CPU_ALL },
    { "include", pseudo_include, 0x0, 0, CPU_ALL },
    { "chksum", pseudo_chksum, 0x0, 0, CPU_ALL },
    { "cpu", pseudo_cpu, 0x0, 0, CPU_ALL },
};

typedef struct Register {
    char *name;
    int n;
} Register;

static Register regs_table[] = {
    { "r0", 0 },
    { "r1", 1 },
    { "r2", 2 },
    { "r3", 3 },
    { "r4", 4 },
    { "r5", 5 },
    { "r6", 6 },
    { "r7", 7 },
    { "sp", 6 },
    { "pc", 7 },
};

typedef struct Macro {
    char *name;
    char **line;
    int lines;
    int args;
    char *arg_name[10];
    struct Macro *prev;
} Macro;

typedef struct Label {
    char *name;
    unsigned int address;
    int line;
    struct Label *prev;
} Label;

typedef struct Proc {
    char *name;
    Label *labels;
    Label *globals;
    Label *equs;
    int line;
    struct Proc *prev;
} Proc;

typedef struct File {
    char *in_file_path;
    FILE *in_file;
    int src_line;
    struct File *prev;
} File;

static char *in_file_path;
static FILE *in_file;

static unsigned char output[65536];
static unsigned int start_addr = 0;
static unsigned int output_addr = 0;

static int use_chksum = 0;
static unsigned int chksum_addr;

static int src_pass = 1;
static int src_line = 1;

static int case_sensitive_symbols = 0;
static int jmp_label_indirect = 0;

typedef struct {
    int active;
    int seen_else;
} IfState;

#define IF_STACK_MAX 32
static IfState if_stack[IF_STACK_MAX];
static int if_sp = 0;

static Label *labels = NULL;
static Label *equs = NULL;
static Proc *procs = NULL;
static Macro *macros = NULL;
static File *files = NULL;
static FILE *list_out = NULL;

static int error = 0;
static int to_second_pass = 0;

static int in_macro = 0;
static Proc *in_proc = NULL;
static int pad_tail_words = 0;
static int emit_is_fill = 0;
static int tail_zero_start = -1;

#define MAX_OUTPUT (65536)

#define SKIP_BLANK(s) { \
    while (*(s) && isblank(*(s))) { \
	(s)++; \
    } \
}

#define SKIP_TOKEN(s) { \
    if (isalpha(*(s)) || *(s) == '_' || *(s) == ':' || *(s) == '.') { \
	(s)++; \
	while (*(s) && (isalnum(*(s)) || *(s) == '_' || *(s) == '$')) { \
	    (s)++; \
	} \
    } \
}

#define REMOVE_ENDLINE(s) { \
    while (*(s)) { \
	if (*(s) == '\n' || *(s) == '\r') *(s) = 0; \
	(s)++; \
    } \
}

#define STRING_TOLOWER(s) { \
    while (*(s)) { \
	*(s) = tolower(*(s)); \
	(s)++; \
    } \
}

#define LIST_WORD_SLOTS 3

static void expand_tabs(const char *src, char *dst, size_t dst_size, int tabstop)
{
    size_t col = 0;
    size_t i = 0;
    if (dst_size == 0) {
        return;
    }
    while (*src && i + 1 < dst_size) {
        if (*src == '\t') {
            int spaces = tabstop - (int)(col % (size_t)tabstop);
            while (spaces-- > 0 && i + 1 < dst_size) {
                dst[i++] = ' ';
                col++;
            }
            src++;
            continue;
        }
        dst[i++] = *src++;
        col++;
    }
    dst[i] = 0;
}

static void list_line_words(int line_no, unsigned int addr,
                            const unsigned short *words, int nwords,
                            const char *line)
{
    if (!list_out) {
        return;
    }
    char line_expanded[1024];
    expand_tabs(line, line_expanded, sizeof(line_expanded), 8);
    fprintf(list_out, "%4d %06o:", line_no, addr);
    for (int i = 0; i < nwords; i++) {
        fprintf(list_out, " %06o", words[i]);
    }
    for (int i = nwords; i < LIST_WORD_SLOTS; i++) {
        fprintf(list_out, "       ");
    }
    fprintf(list_out, "  %s\n", line_expanded);
}

static int emit_byte(unsigned char b)
{
    if (output_addr >= MAX_OUTPUT) {
        error = OUTPUT_BUFFER_OVERFLOW;
        return 0;
    }
    if (emit_is_fill) {
        if (b == 0) {
            if (tail_zero_start < 0) {
                tail_zero_start = (int)output_addr;
            }
        } else {
            tail_zero_start = -1;
        }
    } else {
        tail_zero_start = -1;
    }
    output[output_addr++] = b;
    return 1;
}

static int emit_word(unsigned short w)
{
    if (!emit_byte(w & 0xff)) {
        return 0;
    }
    if (!emit_byte((w >> 8) & 0xff)) {
        return 0;
    }
    return 1;
}

static void remove_comment(char *str)
{
    int q = 0, dq = 0;
    while (*str) {
        if (*str == '\'') {
            q = !q;
        }
        if (*str == '"') {
            dq = !dq;
        }

        if (*str == ';' && (q == 0 || dq == 0)) {
            *str = 0;
            break;
        }

        if (*str == '/' && *(str + 1) && *(str + 1) == '/'
                && (q == 0 || dq == 0)) {
            *str = 0;
            break;
        }
        str++;
    }
}

static int exp_(char **str);
static int match(char **str, char c);

static int symbol_eq(const char *a, const char *b)
{
    return case_sensitive_symbols ? (strcmp(a, b) == 0) : (strcasecmp(a, b) == 0);
}

static Label* find_label(Label **list, char *name)
{
    Label *ptr = *list;

    while (ptr) {
        if (symbol_eq(ptr->name, name)) {
            return ptr;
        }
        ptr = ptr->prev;
    }

    return NULL;
}

static Label* add_label(Label **list, char *name, unsigned int address,
                        int line)
{
    if (find_label(list, name)) {
        error = LABEL_ALREADY_DEFINED;
        return NULL;
    }

    Label *new = malloc(sizeof(Label));
    if (!new) {
        error = NO_MEMORY_FOR_LABEL;
        return NULL;
    }
    new->name = strdup(name);
    if (!new->name) {
        free(new);
        error = NO_MEMORY_FOR_LABEL;
        return NULL;
    }
    new->address = address;
    new->line = line;
    new->prev = *list;

    *list = new;

    return new;
}

static void dump_labels(Label *list)
{
    FILE *out = list_out ? list_out : stderr;
    while (list) {
        fprintf(out, "[%s] %06o\n", list->name, list->address & 0xFFFF);
        list = list->prev;
    }
}

static OpCode* find_opcode(char *name, int *is_byte)
{
    char tmp[64];
    char *p = name;

    if (*p == '.') {
        p++;
    }

    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;

    *is_byte = 0;

    for (int i = 0; i < sizeof(opcode_table) / sizeof(OpCode); i++) {
        if (!strcasecmp(tmp, opcode_table[i].name)) {
            return &opcode_table[i];
        }
    }

    size_t len = strlen(tmp);
    if (len > 1 && (tmp[len - 1] == 'b' || tmp[len - 1] == 'B')) {
        tmp[len - 1] = 0;
        for (int i = 0; i < sizeof(opcode_table) / sizeof(OpCode); i++) {
            if (!strcasecmp(tmp, opcode_table[i].name)) {
                if (!opcode_table[i].allow_byte) {
                    return NULL;
                }
                *is_byte = 1;
                return &opcode_table[i];
            }
        }
    }

    return NULL;
}

static int opcode_supported(const OpCode *op)
{
    if (!op) {
        return 0;
    }
    return (op->cpu_mask & current_cpu) != 0;
}

static Register* find_register(char *name)
{
    for (int i = 0; i < sizeof(regs_table) / sizeof(Register); i++) {
        if (!strcasecmp(name, regs_table[i].name)) {
            return &regs_table[i];
        }
    }

    return NULL;
}

static Register* find_register_in_string(char **str)
{
    char tmp[256];
    char *ptr = tmp;
    char *ptr_str = *str;

    SKIP_BLANK(ptr_str);

    while(*ptr_str && isalnum(*ptr_str)) {
        if (ptr - tmp >= 255) {
            break;
        }
        *ptr++ = *ptr_str++;
    }

    *ptr = 0;

    Register *reg = find_register(tmp);

    if (reg) {
        *str = ptr_str;
    }

    return reg;
}

static int symbol_defined(char *name)
{
    if (find_label(&equs, name) || find_label(&labels, name)) {
        return 1;
    }
    if (in_proc) {
        if (find_label(&in_proc->labels, name) || find_label(&in_proc->equs, name)) {
            return 1;
        }
        if (find_label(&in_proc->globals, name)) {
            return 1;
        }
    }
    return 0;
}

static int is_skipping(void)
{
    for (int i = 0; i < if_sp; i++) {
        if (!if_stack[i].active) {
            return 1;
        }
    }
    return 0;
}

static Macro* find_macro(char *name)
{
    Macro *tmp = macros;
    while (tmp) {
        if (symbol_eq(tmp->name, name)) {
            return tmp;
        }
        tmp = tmp->prev;
    }
    return NULL;
}

static int add_macro(FILE *inf, char *name, char *params)
{
    char str[512];
    Macro *mac;

    if (src_pass == 1) {
        if (find_macro(name)) {
            error = MACRO_ALREADY_DEFINED;
            return 1;
        }

        mac = malloc(sizeof(Macro));
        if (!mac) {
            error = NO_MEMORY_FOR_MACRO;
            return 1;
        }

        mac->name = strdup(name);
        if (!mac->name) {
            free(mac);
            error = NO_MEMORY_FOR_MACRO;
            return 1;
        }
        mac->line = NULL;
        mac->lines = 0;
        mac->args = 0;
        for (int i = 0; i < 10; i++) {
            mac->arg_name[i] = NULL;
        }
        if (params) {
            char *p = params;
            while (*p) {
                SKIP_BLANK(p);
                if (!*p) {
                    break;
                }
                char *start = p;
                SKIP_TOKEN(p);
                char saved = *p;
                *p = 0;
                if (*start) {
                    if (mac->args < 10) {
                        mac->arg_name[mac->args++] = strdup(start);
                    }
                }
                *p = saved;
                if (!match(&p, ',')) {
                    break;
                }
            }
        }
        mac->prev = macros;
    }

    while(fgets(str, sizeof(str), inf)) {
        char tmp[512];
        char *ptr = str;
        REMOVE_ENDLINE(ptr);

        if (src_pass == 2) {
            list_line_words(src_line, output_addr, NULL, 0, str);
        }

        strcpy(tmp, str);
        ptr = tmp;
        SKIP_BLANK(ptr);
        char *ptr1 = ptr;
        REMOVE_ENDLINE(ptr1);
        ptr1 = ptr;
        SKIP_TOKEN(ptr1);
        *ptr1 = 0;
        ptr1 = ptr;
        if (!strcasecmp(ptr, "endm")) {
            break;
        }

        if (src_pass == 1) {
            char **new_line = realloc(mac->line, sizeof(char*) * (mac->lines + 1));
            if (!new_line) {
                error = NO_MEMORY_FOR_MACRO;
                return 1;
            }
            mac->line = new_line;
            mac->line[mac->lines] = strdup(str);
            if (!mac->line[mac->lines]) {
                for (int i = 0; i < mac->lines; i++) {
                    free(mac->line[i]);
                }
                free(mac->line);
                free(mac->name);
                free(mac);
                error = NO_MEMORY_FOR_MACRO;
                return 1;
            }
            mac->lines++;
        }

        src_line++;
    }

    src_line += 2;

    if (src_pass == 1) {
        macros = mac;
    }

    return 0;
}

static Proc* find_proc(Proc **list, char *name)
{
    Proc *ptr = *list;

    while (ptr) {
        if (symbol_eq(ptr->name, name)) {
            return ptr;
        }
        ptr = ptr->prev;
    }

    return NULL;
}

static Proc* add_proc(Proc **list, char *name, int line)
{
    if (find_proc(list, name)) {
        error = PROC_ALREADY_DEFINED;
        return NULL;
    }

    Proc *new = malloc(sizeof(Proc));
    if (!new) {
        error = NO_MEMORY_FOR_PROC;
        return NULL;
    }
    new->name = strdup(name);
    if (!new->name) {
        free(new);
        error = NO_MEMORY_FOR_PROC;
        return NULL;
    }
    new->labels = NULL;
    new->globals = NULL;
    new->equs = NULL;
    new->line = line;
    new->prev = *list;

    *list = new;

    return new;
}

static int match(char **str, char c)
{
    SKIP_BLANK(*str);

    if (*(*str) != c) {
        return 0;
    }

    (*str)++;
    return 1;
}

static int exp2_(char **str);

static int toint(char c)
{
    if (isdigit(c)) {
        return (c - '0');
    } else if (isxdigit(c)) {
        if (isupper(c)) {
            return (c - 'A' + 10);
        } else {
            return (c - 'a' + 10);
        }
    } else {
        error = INVALID_NUMBER;
        return 0;
    }
}

static int hexnum(char **str)
{
    int n = 0;
    if (!isxdigit(*(*str))) {
        error = INVALID_HEX_NUMBER;
        return 0;
    }
    while (isxdigit(*(*str))) {
        n = n * 16 + toint(*(*str)++);
    }
    return n;
}

static int binary(char **str)
{
    int n = 0;
    if (*(*str) != '0' && *(*str) != '1') {
        error = INVALID_BINARY_NUMBER;
        return 0;
    }
    while (*(*str) == '0' || *(*str) == '1') {
        n = n * 2 + *(*str)++ - '0';
    }
    return n;
}

static int decimal_with_dot(char **str)
{
    int n = 0;
    if (!isdigit(*(*str))) {
        error = INVALID_DECIMAL_NUMBER;
        return 0;
    }
    while (isdigit(*(*str))) {
        n = n * 10 + *(*str)++ - '0';
    }
    if (*(*str) == '.') {
        (*str)++;
        return n;
    }
    return n;
}

static int octal_default(char **str)
{
    int n = 0;
    if (*(*str) < '0' || *(*str) > '7') {
        error = INVALID_OCTAL_NUMBER;
        return 0;
    }
    while (*(*str) >= '0' && *(*str) <= '7') {
        n = n * 8 + *(*str)++ - '0';
    }
    return n;
}

static int number(char **str)
{
    if (*(*str) == '0' && (*(*str + 1) == 'd' || *(*str + 1) == 'D')) {
        *str += 2;
        return decimal_with_dot(str);
    }
    if (*(*str) == '0' && (*(*str + 1) == 'x' || *(*str + 1) == 'X')) {
        *str += 2;
        return hexnum(str);
    }
    if (*(*str) == '0' && (*(*str + 1) == 'b' || *(*str + 1) == 'B')) {
        *str += 2;
        return binary(str);
    }
    return octal_default(str);
}

static int character(char **str)
{
    char c;

    c = *(*str)++;
    if (*(*str) == '\'') {
        (*str)++;
    }
    return c;
}

static int operand(char **str)
{
    char *ptr = *str;
    char tmp[256];
    char *ptr1 = tmp;

    while (*ptr && (isalnum(*ptr) || *ptr == '_' || *ptr == ':' || *ptr == '.' || *ptr == '$')) {
        if (ptr1 - tmp >= 255) {
            error = SYNTAX_ERROR;
            return 0;
        }
        *ptr1++ = *ptr++;
    }
    *ptr1 = 0;

    Label *label = NULL;

    if (in_proc) {
        label = find_label(&in_proc->labels, tmp);

        if (!label) {
            label = find_label(&in_proc->equs, tmp);
        }
    }

    if (!label) {
        label = find_label(&labels, tmp);
    }

    if (!label) {
        label = find_label(&equs, tmp);
    }

    if (label) {
        *str = ptr;
        return label->address;
    } else if (match(str, '$')) {
        return hexnum(str);
    } else if (match(str, '@')) {
        return octal_default(str);
    } else if (match(str, '%')) {
        return binary(str);
    } else if (match(str, '\'')) {
        return character(str);
    } else if (match(str, '*')) {
        return output_addr;
    } else if (isdigit(*(*str))) {
        char *tmp = *str;
        if (*tmp == '0' && (*(tmp + 1) == 'x' || *(tmp + 1) == 'X' ||
                            *(tmp + 1) == 'b' || *(tmp + 1) == 'B' ||
                            *(tmp + 1) == 'd' || *(tmp + 1) == 'D')) {
            return number(str);
        }
        while (isdigit(*tmp)) {
            tmp++;
        }
        if (*tmp == '.') {
            return decimal_with_dot(str);
        }
        return octal_default(str);
    } else {
        *str = ptr;
        if (src_pass == 2) {
            error = CANNOT_RESOLVE_REF;
        } else {
            to_second_pass = 1;
        }
        return 0;
    }
}

static int exp8(char **str)
{
    int n;
    if (match(str, '(')) {
        n = exp2_(str);
        if (match(str, ')')) {
            return n;
        } else {
            error = MISSED_BRACKET;
            return 0;
        }
    }
    return operand(str);
}

static int exp7(char **str)
{
    if (match(str, '~')) {
        return 0xFFFF ^ exp8(str);
    }
    if (match(str, '-')) {
        return -exp8(str);
    }
    return exp8(str);
}

static int exp6(char **str)
{
    int n;
    n = exp7(str);
    while (*(*str)) {
        if (match(str, '*')) {
            n = n * exp7(str);
        } else if (match(str, '/')) {
            int divisor = exp7(str);
            if (divisor == 0) {
                error = SYNTAX_ERROR;
                return 0;
            }
            n = n / divisor;
        } else if (match(str, '%')) {
            int divisor = exp7(str);
            if (divisor == 0) {
                error = SYNTAX_ERROR;
                return 0;
            }
            n = n % divisor;
        } else {
            break;
        }
    }
    return n;
}

static int exp5(char **str)
{
    int n;
    n = exp6(str);
    while (*(*str)) {
        if (match(str, '+')) {
            n = n + exp6(str);
        } else if (match(str, '-')) {
            n = n - exp6(str);
        } else {
            break;
        }
    }
    return n;
}

static int exp4(char **str)
{
    int n;
    n = exp5(str);
    while (*(*str)) {
        if (match(str, '&')) {
            n = n & exp5(str);
        } else {
            break;
        }
    }
    return n;
}

static int exp3(char **str)
{
    int n;
    n = exp4(str);
    while (*(*str)) {
        if (match(str, '^')) {
            n = n ^ exp4(str);
        } else {
            break;
        }
    }
    return n;
}

static int exp2_(char **str)
{
    int n;
    n = exp3(str);
    while (*(*str)) {
        if (match(str, '|')) {
            n = n | exp3(str);
        } else {
            break;
        }
    }
    return n;
}

static int exp_(char **str)
{
    if (match(str, '/')) {
        return (exp2_(str) >> 8);
    } else {
        return (exp2_(str));
    }
}

typedef struct Operand {
    int mode;
    int reg;
    int has_ext;
    int ext;
    int pc_relative;
} Operand;

static int operand_spec(Operand *op)
{
    return ((op->mode & 0x07) << 3) | (op->reg & 0x07);
}

static int parse_register(char **str, int *out_reg)
{
    Register *reg = find_register_in_string(str);
    if (!reg) {
        return 0;
    }
    *out_reg = reg->n;
    return 1;
}

static int parse_operand(char **str, Operand *op)
{
    int deferred = 0;
    char *ptr = *str;

    SKIP_BLANK(ptr);

    if (match(&ptr, '@')) {
        deferred = 1;
    }

    if (match(&ptr, '#')) {
        op->mode = deferred ? 3 : 2;
        op->reg = 7;
        op->has_ext = 1;
        op->ext = exp_(&ptr);
        op->pc_relative = 0;
        *str = ptr;
        return 1;
    }

    if (match(&ptr, '-')) {
        if (!match(&ptr, '(')) {
            error = SYNTAX_ERROR;
            return 0;
        }
        if (!parse_register(&ptr, &op->reg)) {
            error = MISSED_REGISTER_ARG_2;
            return 0;
        }
        if (!match(&ptr, ')')) {
            error = MISSED_BRACKET;
            return 0;
        }
        op->mode = deferred ? 5 : 4;
        op->has_ext = 0;
        op->pc_relative = 0;
        *str = ptr;
        return 1;
    }

    if (match(&ptr, '(')) {
        if (!parse_register(&ptr, &op->reg)) {
            error = MISSED_REGISTER_ARG_2;
            return 0;
        }
        if (!match(&ptr, ')')) {
            error = MISSED_BRACKET;
            return 0;
        }
        if (match(&ptr, '+')) {
            op->mode = deferred ? 3 : 2;
        } else {
            op->mode = 1;
        }
        op->has_ext = 0;
        op->pc_relative = 0;
        *str = ptr;
        return 1;
    }

    if (deferred) {
        int reg;
        char *tmp = ptr;
        if (parse_register(&tmp, &reg)) {
            op->mode = 1;
            op->reg = reg;
            op->has_ext = 0;
            op->pc_relative = 0;
            *str = tmp;
            return 1;
        }
    }

    {
        char *tmp = ptr;
        int reg;
        if (parse_register(&tmp, &reg)) {
            op->mode = 0;
            op->reg = reg;
            op->has_ext = 0;
            op->pc_relative = 0;
            *str = tmp;
            return 1;
        }
    }

    {
        char *tmp = ptr;
        int val = exp_(&tmp);
        int has_symbol = 0;
        for (char *p = ptr; p < tmp; p++) {
            if (isalpha(*p) || *p == '_' || *p == '.' || *p == ':' || *p == '$') {
                has_symbol = 1;
                break;
            }
        }
        SKIP_BLANK(tmp);
        if (match(&tmp, '(')) {
            if (!parse_register(&tmp, &op->reg)) {
                error = MISSED_REGISTER_ARG_2;
                return 0;
            }
            if (!match(&tmp, ')')) {
                error = MISSED_BRACKET;
                return 0;
            }
            op->mode = deferred ? 7 : 6;
            op->has_ext = 1;
            op->ext = val;
            op->pc_relative = (op->reg == 7) ? has_symbol : 0;
            *str = tmp;
            return 1;
        }

        op->mode = deferred ? 7 : 6;
        op->reg = 7;
        op->has_ext = 1;
        op->ext = val;
        op->pc_relative = 1;
        *str = tmp;
        return 1;
    }
}

static int get_bytes(char *str)
{
    char delim = 0;
    int old_addr = output_addr;

    SKIP_BLANK(str);
    while (*str) {
        if (delim) {
            if (*str == 0 || *str == '\n' || *str == '\r') {
                break;
            }
            if (*str != delim) {
                if (delim == '\'') {
                    emit_byte(0);
                    str++;
                } else if (*str == '\\') {
                    str++;
                    if (*str == 'n') {
                        emit_byte('\n');
                    } else if (*str == 'r') {
                        emit_byte('\r');
                    } else if (*str == 't') {
                        emit_byte('\t');
                    } else if (*str == '0') {
                        emit_byte('\0');
                    } else if (*str == '\\') {
                        emit_byte('\\');
                    } else if (*str == '\"') {
                        emit_byte('\"');
                    } else if (*str == '\'') {
                        emit_byte('\'');
                    } else if (*str) {
                        emit_byte(*str);
                    }
                    if (*str) {
                        str++;
                    }
                } else {
                    emit_byte(*str++);
                }
                continue;
            }
            delim = 0;
            str++;
        } else if (*str == '"' || *str == '\'') {
            delim = *str++;
            continue;
        } else {
            emit_byte(exp_(&str) & 0xFF);
        }
        if (match(&str, ',') == 0) {
            break;
        }
        SKIP_BLANK(str);
    }
    if (delim) {
        error = EXPECTED_CLOSE_QUOTE;
    }

    return output_addr - old_addr;
}

static int get_words(char *str)
{
    int old_addr = output_addr;

    while (*str) {
        char *tmp = str;
        int word = exp_(&str);
        if (src_pass == 2 && !pad_tail_words) {
            int has_minus = 0;
            int has_alpha = 0;
            for (char *p = tmp; p < str; p++) {
                if (*p == '-') {
                    has_minus = 1;
                } else if (isalpha((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$' || *p == ':') {
                    has_alpha = 1;
                }
            }
            if (has_minus && has_alpha) {
                pad_tail_words = 1;
            }
        }
        emit_word(word & 0xFFFF);
        if (match(&str, ',') == 0) {
            break;
        }
        SKIP_BLANK(str);
    }

    return output_addr - old_addr;
}

static int do_asm(FILE *inf, char *str);

static int is_ident_start(int c)
{
    return isalpha(c) || c == '_' || c == '.' || c == '$';
}

static int is_ident_char(int c)
{
    return isalnum(c) || c == '_' || c == '$';
}

static void expand_named_params(const Macro *mac, char **arg, const char *src, char *dst, size_t dst_size)
{
    const char *p = src;
    char *d = dst;
    size_t left = dst_size ? dst_size - 1 : 0;

    while (*p && left > 0) {
        if (is_ident_start((unsigned char)*p)) {
            char tok[64];
            size_t tlen = 0;
            const char *start = p;
            while (*p && is_ident_char((unsigned char)*p) && tlen < sizeof(tok) - 1) {
                tok[tlen++] = *p++;
            }
            tok[tlen] = 0;
            int replaced = 0;
            for (int i = 0; i < mac->args; i++) {
                if (mac->arg_name[i] && !strcasecmp(tok, mac->arg_name[i]) && arg[i]) {
                    size_t alen = strlen(arg[i]);
                    if (alen > left) {
                        alen = left;
                    }
                    memcpy(d, arg[i], alen);
                    d += alen;
                    left -= alen;
                    replaced = 1;
                    break;
                }
            }
            if (!replaced) {
                size_t copy = (size_t)(p - start);
                if (copy > left) {
                    copy = left;
                }
                memcpy(d, start, copy);
                d += copy;
                left -= copy;
            }
        } else {
            *d++ = *p++;
            left--;
        }
    }
    *d = 0;
}

static int expand_macro(FILE *inf, Macro *mac, char *args)
{
    int i = 0;
    char *arg[10];

    src_line++;

    in_macro++;

    // parse args
    while (args && *args) {
        SKIP_BLANK(args);
        arg[i++] = args;
        while (*args && *args != ',') {
            args++;
        }

        if (*args == ',') {
            *args++ = 0;
            continue;
        }
    }

    arg[i] = NULL;

//	for (i = 0; arg[i]; i++) {
//	    fprintf(stderr, "[%s]\n", arg[i]);
//	}

    for (i = 0; i < mac->lines; i++) {
        char line[1024];
        char line2[1024];
        char *ptr_tmp;
        char *ptr_src = mac->line[i];
        char *ptr_dst = line;

        if (!strchr(ptr_src, '#')) {
            strcpy(line, ptr_src);
        } else {
            while ((ptr_tmp = strchr(ptr_src, '#'))) {
                if (isdigit(*(ptr_tmp + 1))) {
                    int len = (ptr_tmp - ptr_src);
                    int n = *(ptr_tmp + 1) - '0' - 1;

                    strncpy(ptr_dst, ptr_src, len);
                    strcpy(ptr_dst + len, arg[n]);
                    ptr_dst += strlen(ptr_dst);
                    ptr_src = ptr_tmp + 2;
                } else {
                    break;
                }
            }
            if (*ptr_src) {
                strcpy(ptr_dst, ptr_src);
            }
        }

        if (mac->args > 0) {
            expand_named_params(mac, arg, line, line2, sizeof(line2));
        } else {
            strcpy(line2, line);
        }

        if (src_pass == 2) {
            fprintf(stderr, "[%s]:%d ", mac->name, i + 1);
        }

        int ret = do_asm(inf, line2);
        if (ret) {
            if (src_pass == 1) {
                fprintf(stderr, "[%s]:%d %s\n", mac->name, i + 1, line2);
            }
            return ret;
        }
    }

    in_macro--;

    return 0;
}

static char *get_file_path(char *name)
{
    char *tmp;
    if ((tmp = strrchr(name, '/'))) {
        *tmp = 0;
    } else {
        strcpy(name, ".");
    }
    return name;
}

static int do_asm(FILE *inf, char *line)
{
    char last;
    char *ptr, *ptr1;
    char linetmp[strlen(line) + 1];
    char *str = linetmp;
    int list_line = src_line;

    strcpy(linetmp, line);

    remove_comment(str);

    SKIP_BLANK(str);

    {
        char *scan = str;
        SKIP_BLANK(scan);
        if (*scan) {
            char *tok_start = scan;
            SKIP_TOKEN(scan);
            char saved = *scan;
            *scan = 0;
            char *tok = tok_start;
            if (*tok == '.') {
                tok++;
            }
            int is_conditional = (!strcasecmp(tok, "if") ||
                                  !strcasecmp(tok, "ifdef") ||
                                  !strcasecmp(tok, "ifndef") ||
                                  !strcasecmp(tok, "else") ||
                                  !strcasecmp(tok, "endif"));
            if (is_conditional) {
                char *args = saved ? (scan + 1) : scan;
                if (!strcasecmp(tok, "if")) {
                    int parent_active = is_skipping() ? 0 : 1;
                    int cond = parent_active ? (exp_(&args) != 0) : 0;
                    if (if_sp >= IF_STACK_MAX) {
                        error = SYNTAX_ERROR;
                        return 1;
                    }
                    if_stack[if_sp].active = cond;
                    if_stack[if_sp].seen_else = 0;
                    if_sp++;
                } else if (!strcasecmp(tok, "ifdef") || !strcasecmp(tok, "ifndef")) {
                    int parent_active = is_skipping() ? 0 : 1;
                    char *p = args;
                    SKIP_BLANK(p);
                    char *name = p;
                    SKIP_TOKEN(p);
                    *p = 0;
                    int defined = symbol_defined(name);
                    int cond = parent_active ? (strcasecmp(tok, "ifdef") == 0 ? defined : !defined) : 0;
                    if (if_sp >= IF_STACK_MAX) {
                        error = SYNTAX_ERROR;
                        return 1;
                    }
                    if_stack[if_sp].active = cond;
                    if_stack[if_sp].seen_else = 0;
                    if_sp++;
                } else if (!strcasecmp(tok, "else")) {
                    if (if_sp == 0) {
                        error = SYNTAX_ERROR;
                        return 1;
                    }
                    if (if_stack[if_sp - 1].seen_else) {
                        error = SYNTAX_ERROR;
                        return 1;
                    }
                    int parent_active = 1;
                    for (int i = 0; i < if_sp - 1; i++) {
                        if (!if_stack[i].active) {
                            parent_active = 0;
                            break;
                        }
                    }
                    if_stack[if_sp - 1].active = parent_active ? !if_stack[if_sp - 1].active : 0;
                    if_stack[if_sp - 1].seen_else = 1;
                } else if (!strcasecmp(tok, "endif")) {
                    if (if_sp == 0) {
                        error = SYNTAX_ERROR;
                        return 1;
                    }
                    if_sp--;
                }
                return 0;
            } else {
                *scan = saved;
            }
        }
    }

    if (is_skipping()) {
        if (!in_macro) {
            src_line++;
        }
        return 0;
    }

    ptr = str;
    SKIP_TOKEN(str);
    ptr1 = str;

    if ((last = *ptr1)) {
        str++;
    }
    *ptr1 = 0;

    if (ptr1 - ptr > 0) {
        char *label = NULL;
        char *first_tok = ptr;

        OpCode *opcode = NULL;
        int is_byte = 0;
        Macro *mac = NULL;

        if (last == ':') {
            label = first_tok;
            SKIP_BLANK(str);
            ptr = str;
            SKIP_TOKEN(str);
            ptr1 = str;

            if ((last = *ptr1)) {
                str++;
            }
            *ptr1 = 0;

            if (ptr1 - ptr > 0) {
                mac = find_macro(ptr);
                if (!mac) {
                    opcode = find_opcode(ptr, &is_byte);
                }
            }
        } else {
            mac = find_macro(first_tok);
            if (!mac) {
                opcode = find_opcode(first_tok, &is_byte);
            }

            if (!mac && !opcode) {
                label = first_tok;
                if (last) {
                    SKIP_BLANK(str);

                    ptr = str;
                    SKIP_TOKEN(str);
                    ptr1 = str;

                    if ((last = *ptr1)) {
                        str++;
                    }
                    *ptr1 = 0;

                    mac = find_macro(ptr);
                    if (!mac) {
                        opcode = find_opcode(ptr, &is_byte);
                    }
                } else {
                    ptr = str;
                }
            }
        }

        if (opcode && !opcode_supported(opcode) && opcode->type < pseudo_db) {
            error = UNSUPPORTED_INSTRUCTION;
            return 1;
        }

        if (label && src_pass == 1 &&
                (mac || !(opcode && !strcasecmp(opcode->name, "equ")))) {
            if (in_proc) {
                Label *global = find_label(&in_proc->globals, label);
                if (global) {
                    add_label(&labels, label, output_addr, src_line);
                } else {
                    add_label(&in_proc->labels, label, output_addr, src_line);
                }
            } else {
                add_label(&labels, label, output_addr, src_line);
            }
        }

        if (mac) {
            if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
            SKIP_BLANK(str);
            return expand_macro(inf, mac, last ? str : NULL);
        }

//fprintf(stderr, ">>>%s\n", line);
//fprintf(stderr, "OPCODE: %s %d %X %X\n", opcode->name, opcode->type, opcode->op, opcode->ext_op);

        if (opcode && !strcmp(opcode->name, "include")) {
            char name[512];
            if (label) {
                error = SYNTAX_ERROR;
                return 1;
            }
            File *file = malloc(sizeof(File));
            file->src_line = src_line + 1;
            file->in_file_path = in_file_path;
            file->in_file = in_file;
            file->prev = files;
            files = file;
            src_line = 1;
            SKIP_BLANK(str);
            if (*str == '\"' || *str == '\'') {
                char quote = *str++;
                char *end = strrchr(str, quote);
                if (end) {
                    *end = 0;
                }
            }
            snprintf(name, sizeof(name), "%s/%s", in_file_path, str);
            fprintf(stderr, "\r%s\n", name);
            in_file = fopen(name, "rb");
            if (!in_file) {
                error = CANNOT_OPEN_FILE;
                return 1;
            }
            {
                char dirbuf[512];
                strncpy(dirbuf, name, sizeof(dirbuf) - 1);
                dirbuf[sizeof(dirbuf) - 1] = 0;
                get_file_path(dirbuf);
                in_file_path = strdup(dirbuf);
            }
            return 0;
        } else if (opcode && !strcmp(opcode->name, "equ")) {
            if (!label) {
                error = MISSED_NAME_FOR_EQU;
            } else {
                SKIP_BLANK(str);
                unsigned int val = exp_(&str);
                if (src_pass == 2) {
                    if (in_proc) {
                        add_label(&in_proc->equs, label, val, src_line);
                    } else {
                        add_label(&equs, label, val, src_line);
                    }
                }

                if (src_pass == 2) {
                    unsigned short w = val & 0xFFFF;
                    list_line_words(list_line, output_addr, &w, 1, line);
                }
            }
        } else if (opcode && !strcmp(opcode->name, "proc")) {
            if (!label) {
                error = MISSED_NAME_FOR_PROC;
            } else {
                if (in_proc) {
                    error = NESTED_PROC_UNSUPPORTED;
                } else {
                    in_proc = find_proc(&procs, label);
                    if (!in_proc) {
                        in_proc = add_proc(&procs, label, src_line);
                    }
                }
                if (src_pass == 2) {
                    list_line_words(list_line, output_addr, NULL, 0, line);
                }
            }
        } else if (opcode && !strcmp(opcode->name, "endp")) {
            in_proc = NULL;

            if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
        } else if (opcode && !strcmp(opcode->name, "global")) {
            if (!in_proc) {
                error = ONLY_INSIDE_PROC;
            } else if (src_pass == 1) {
                do {
                    SKIP_BLANK(str);
                    char *name = str;
                    SKIP_TOKEN(str);
                    if ((last = *str)) {
                        *str++ = 0;
                    }
                    add_label(&in_proc->globals, name, output_addr, src_line);
                } while (*str && (last == ',' || match(&str, ',') == 1));

                if (src_pass == 2) {
                    list_line_words(list_line, output_addr, NULL, 0, line);
                }
            }
        } else if (opcode && !strcmp(opcode->name, "macro")) {
            SKIP_BLANK(str);
            char *name = str;
            SKIP_TOKEN(str);
            char *params = NULL;
            if (*str) {
                *str++ = 0;
                params = str;
            } else {
                *str = 0;
            }
            if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
            return add_macro(inf, name, params);
        } else if (opcode && !strcmp(opcode->name, "org")) {
            SKIP_BLANK(str);
            start_addr = exp_(&str);
            output_addr = start_addr;
            if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
        } else if (opcode && opcode->type == pseudo_cpu) {
            if (label) {
                error = SYNTAX_ERROR;
                return 1;
            }
            SKIP_BLANK(str);
            if (!*str) {
                error = SYNTAX_ERROR;
                return 1;
            }
            char name_buf[64];
            if (*str == '\"' || *str == '\'') {
                char quote = *str++;
                char *end = strrchr(str, quote);
                if (!end) {
                    error = EXPECTED_CLOSE_QUOTE;
                    return 1;
                }
                *end = 0;
                strncpy(name_buf, str, sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = 0;
            } else {
                char *p = str;
                int i = 0;
                while (*p && !isblank((unsigned char)*p) && *p != ',' && i < (int)sizeof(name_buf) - 1) {
                    name_buf[i++] = *p++;
                }
                name_buf[i] = 0;
            }
            if (!set_cpu_by_name(name_buf)) {
                error = SYNTAX_ERROR;
                return 1;
            }
            if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
        } else if (opcode && opcode->type == pseudo_chksum) {
            use_chksum = 1;
            chksum_addr = output_addr;
            emit_word(0);
            if (src_pass == 2) {
                unsigned short w = 0;
                list_line_words(list_line, output_addr - 2, &w, 1, line);
            }
        } else if (opcode) {
            unsigned int old_addr = output_addr;
            unsigned short word = 0;
            Operand src_op;
            Operand dst_op;

            if (opcode->type == pseudo_db) {
                get_bytes(str);
            } else if (opcode->type == pseudo_dw) {
                get_words(str);
            } else if (opcode->type == pseudo_ds
                       || opcode->type == pseudo_dsw
                       || opcode->type == pseudo_align) {
                int count;
                int fill = 0;

                if (opcode->type == pseudo_align && !strcmp(opcode->name, "even")) {
                    SKIP_BLANK(str);
                    if (*str) {
                        error = EXTRA_SYMBOLS;
                        return 1;
                    }
                    count = 1;
                } else {
                    count = exp_(&str);
                }

                if (match(&str, ',')) {
                    int val = exp_(&str) & 0xFFFF;
                    if (opcode->type == pseudo_ds) {
                        fill = val & 0xFF;
                    } else if (opcode->type == pseudo_dsw) {
                        fill = val & 0xFFFF;
                    } else {
                        fill = val & 0xFF;
                    }
                }

                if (opcode->type == pseudo_align) {
                    int n = 1 << count;
                    if (n > 1) {
                        n = n - 1;
                    }
                    count = ((output_addr + n) & ~n) - output_addr;
                }

                if (opcode->type == pseudo_dsw) {
                    emit_is_fill = (fill == 0);
                    while (count-- > 0) {
                        emit_word(fill);
                    }
                    emit_is_fill = 0;
                } else {
                    emit_is_fill = (fill == 0);
                    while (count-- > 0) {
                        emit_byte(fill);
                    }
                    emit_is_fill = 0;
                }
            } else if (opcode->type == op_none || opcode->type == op_ccode) {
                SKIP_BLANK(str);
                if (*str) {
                    error = EXTRA_SYMBOLS;
                    return 1;
                }
                word = opcode->base;
                emit_word(word);
            } else if (opcode->type == op_branch) {
                SKIP_BLANK(str);
                int val = exp_(&str);
                int offset = (val - (int)(old_addr + 2)) / 2;
                if (src_pass == 2 && (offset < -128 || offset > 127)) {
                    error = LONG_RELATED_OFFSET;
                    return 1;
                }
                word = opcode->base | (offset & 0xFF);
                emit_word(word);
            } else if (opcode->type == op_jmp) {
                if (!parse_operand(&str, &dst_op)) {
                    return 1;
                }
                if (dst_op.mode == 0) {
                    error = SYNTAX_ERROR;
                    return 1;
                }
                if (jmp_label_indirect && dst_op.pc_relative && dst_op.reg == 7 && dst_op.mode == 6) {
                    dst_op.mode = 7;
                }
                word = opcode->base | operand_spec(&dst_op);
                emit_word(word);
                if (dst_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = dst_op.ext;
                    if (dst_op.pc_relative) {
                        ext_val = dst_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else if (opcode->type == op_jsr) {
                int reg;
                SKIP_BLANK(str);
                if (!parse_register(&str, &reg)) {
                    error = MISSED_OPCODE_ARG_1;
                    return 1;
                }
                if (match(&str, ',') == 0) {
                    error = EXPECTED_ARG_2;
                    return 1;
                }
                if (!parse_operand(&str, &dst_op)) {
                    return 1;
                }
                word = opcode->base | ((reg & 0x07) << 6) | operand_spec(&dst_op);
                emit_word(word);
                if (dst_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = dst_op.ext;
                    if (dst_op.pc_relative) {
                        ext_val = dst_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else if (opcode->type == op_rts) {
                int reg;
                SKIP_BLANK(str);
                if (!parse_register(&str, &reg)) {
                    error = MISSED_OPCODE_ARG_1;
                    return 1;
                }
                word = opcode->base | (reg & 0x07);
                emit_word(word);
            } else if (opcode->type == op_sob) {
                int reg;
                int val;
                SKIP_BLANK(str);
                if (!parse_register(&str, &reg)) {
                    error = MISSED_OPCODE_ARG_1;
                    return 1;
                }
                if (match(&str, ',') == 0) {
                    error = EXPECTED_ARG_2;
                    return 1;
                }
                val = exp_(&str);
                int offset = ((int)(old_addr + 2) - val) / 2;
                if (src_pass == 2 && (offset < 0 || offset > 63)) {
                    error = LONG_RELATED_OFFSET;
                    return 1;
                }
                word = opcode->base | ((reg & 0x07) << 6) | (offset & 0x3F);
                emit_word(word);
            } else if (opcode->type == op_mark) {
                SKIP_BLANK(str);
                int val = exp_(&str);
                if (val < 0 || val > 63) {
                    error = SYNTAX_ERROR;
                    return 1;
                }
                word = opcode->base | (val & 0x3F);
                emit_word(word);
            } else if (opcode->type == op_eis) {
                int reg;
                SKIP_BLANK(str);
                if (!parse_operand(&str, &src_op)) {
                    return 1;
                }
                if (match(&str, ',') == 0) {
                    error = EXPECTED_ARG_2;
                    return 1;
                }
                if (!parse_register(&str, &reg)) {
                    error = MISSED_REGISTER_ARG_2;
                    return 1;
                }
                word = opcode->base | ((reg & 0x07) << 6) | operand_spec(&src_op);
                emit_word(word);
                if (src_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = src_op.ext;
                    if (src_op.pc_relative) {
                        ext_val = src_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else if (opcode->type == op_xor) {
                int reg;
                SKIP_BLANK(str);
                if (!parse_register(&str, &reg)) {
                    error = MISSED_OPCODE_ARG_1;
                    return 1;
                }
                if (match(&str, ',') == 0) {
                    error = EXPECTED_ARG_2;
                    return 1;
                }
                if (!parse_operand(&str, &dst_op)) {
                    return 1;
                }
                word = opcode->base | ((reg & 0x07) << 6) | operand_spec(&dst_op);
                emit_word(word);
                if (dst_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = dst_op.ext;
                    if (dst_op.pc_relative) {
                        ext_val = dst_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else if (opcode->type == op_fis) {
                int reg;
                SKIP_BLANK(str);
                if (!parse_register(&str, &reg)) {
                    error = MISSED_OPCODE_ARG_1;
                    return 1;
                }
                word = opcode->base | (reg & 0x07);
                emit_word(word);
            } else if (opcode->type == op_trap || opcode->type == op_emt) {
                SKIP_BLANK(str);
                int val = exp_(&str);
                word = opcode->base | (val & 0xFF);
                emit_word(word);
            } else if (opcode->type == op_spl) {
                SKIP_BLANK(str);
                int val = exp_(&str);
                word = opcode->base | (val & 0x07);
                emit_word(word);
            } else if (opcode->type == op_single) {
                if (!parse_operand(&str, &dst_op)) {
                    return 1;
                }
                word = opcode->base | operand_spec(&dst_op);
                if (is_byte) {
                    word |= 0100000;
                }
                emit_word(word);
                if (dst_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = dst_op.ext;
                    if (dst_op.pc_relative) {
                        ext_val = dst_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else if (opcode->type == op_double) {
                if (!parse_operand(&str, &src_op)) {
                    return 1;
                }
                if (match(&str, ',') == 0) {
                    error = EXPECTED_ARG_2;
                    return 1;
                }
                if (!parse_operand(&str, &dst_op)) {
                    return 1;
                }
                word = opcode->base | (operand_spec(&src_op) << 6) | operand_spec(&dst_op);
                if (is_byte) {
                    word |= 0100000;
                }
                emit_word(word);
                if (src_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = src_op.ext;
                    if (src_op.pc_relative) {
                        ext_val = src_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
                if (dst_op.has_ext) {
                    unsigned int ext_addr = output_addr;
                    int ext_val = dst_op.ext;
                    if (dst_op.pc_relative) {
                        ext_val = dst_op.ext - (int)(ext_addr + 2);
                    }
                    emit_word(ext_val & 0xFFFF);
                }
            } else {
                error = SYNTAX_ERROR;
                return 1;
            }

            if (opcode->type == pseudo_db || opcode->type == pseudo_ds
                    || opcode->type == pseudo_align) {
                if (src_pass == 2 && list_out) {
                    int i;
                    list_line_words(list_line, old_addr, NULL, 0, line);
                    for (i = 0; i < output_addr - old_addr; i++) {
                        if ((i % 8) == 0) {
                            if (i == 0) {
                                fprintf(list_out, "%4d %06o:", list_line, old_addr + i);
                            } else {
                                fprintf(list_out, "            ");
                            }
                        }

                        fprintf(list_out, " %03o", output[old_addr + i]);

                        if ((i % 8) == 7) {
                            fprintf(list_out, "\n");
                        }
                    }

                    if ((i % 8) != 0) {
                        fprintf(list_out, "\n");
                    }
                }
            } else if (opcode->type == pseudo_dw) {
                if (src_pass == 2 && list_out) {
                    int i;
                    list_line_words(list_line, old_addr, NULL, 0, line);
                    for (i = 0; i < output_addr - old_addr; i += 2) {
                        if ((i % 8) == 0) {
                            if (i == 0) {
                                fprintf(list_out, "%4d %06o:", list_line, old_addr + i);
                            } else {
                                fprintf(list_out, "            ");
                            }
                        }

                        unsigned short w = (output[old_addr + i + 1] << 8) | output[old_addr + i];
                        fprintf(list_out, " %06o", w);

                        if ((i % 8) == 6) {
                            fprintf(list_out, "\n");
                        }
                    }

                    if ((i % 8) != 0) {
                        fprintf(list_out, "\n");
                    }
                }
            } else if (opcode->type == pseudo_dsw) {
                if (src_pass == 2 && list_out) {
                    int i;
                    list_line_words(list_line, old_addr, NULL, 0, line);
                    for (i = 0; i < output_addr - old_addr; i += 2) {
                        if ((i % 8) == 0) {
                            if (i == 0) {
                                fprintf(list_out, "%4d %06o:", list_line, old_addr + i);
                            } else {
                                fprintf(list_out, "            ");
                            }
                        }

                        unsigned short w = (output[old_addr + i + 1] << 8) | output[old_addr + i];
                        fprintf(list_out, " %06o", w);

                        if ((i % 8) == 6) {
                            fprintf(list_out, "\n");
                        }
                    }

                    if ((i % 8) != 0) {
                        fprintf(list_out, "\n");
                    }
                }
            } else if (opcode->type != pseudo_dw) {
                if (src_pass == 2 && list_out) {
                    int nwords = (output_addr - old_addr) / 2;
                    unsigned short words[8];
                    if (nwords > (int)(sizeof(words) / sizeof(words[0]))) {
                        nwords = (int)(sizeof(words) / sizeof(words[0]));
                    }
                    for (int i = 0; i < nwords; i++) {
                        words[i] = (output[old_addr + i * 2 + 1] << 8) | output[old_addr + i * 2];
                    }
                    list_line_words(list_line, old_addr, words, nwords, line);
                }
            }

            if (opcode->type < pseudo_db) {
                SKIP_BLANK(str);
                if (*str) {
                    error = EXTRA_SYMBOLS;
                    return 1;
                }
            }

        } else {
            if (strlen(ptr)) {
                error = SYNTAX_ERROR;
                return 1;
            } else if (src_pass == 2) {
                list_line_words(list_line, output_addr, NULL, 0, line);
            }
        }
    } else {
        if (src_pass == 2) {
            list_line_words(list_line, output_addr, NULL, 0, line);
        }
    }

    if (!in_macro) {
        src_line++;
    }

    return 0;
}

static void output_hex(FILE *outf)
{
    int i;
    unsigned int out_end = (tail_zero_start >= 0) ? (unsigned int)tail_zero_start : output_addr;

    for (i = start_addr; i < out_end; i++) {
        if ((i % 16) == 0) {
            fprintf(outf, "%04X:", i);
        }

        fprintf(outf, " %02X", output[i]);

        if ((i % 16) == 15) {
            fprintf(outf, "\n");
        }
    }

    if ((i % 16) != 0) {
        fprintf(outf, "\n");
    }
}

static void output_verilog(FILE *outf)
{
    fprintf(outf, "module sram(\n"											\
            "    input  [7:0] ADDR,\n"									\
            "    input  [7:0] DI,\n"									\
            "    output [7:0] DO,\n"									\
            "    input        RW,\n"									\
            "    input        CS\n"										\
            ");\n"														\
            "    parameter  AddressSize = 8;\n"							\
            "    reg        [7:0]    Mem[(1 << AddressSize) - 1:0];\n"	\
            "\n"														\
            "    initial begin\n");

    unsigned int out_end = (tail_zero_start >= 0) ? (unsigned int)tail_zero_start : output_addr;
    for (unsigned int i = start_addr; i < out_end; i++) {
        fprintf(outf, "        Mem[%d] = 8'h%02x;\n", i, output[i]);
    }

    fprintf(outf, "    end\n"													\
            "\n"														\
            "    assign DO = RW ? Mem[ADDR] : 8'hFF;\n"					\
            "\n"														\
            "    always @(CS || RW) begin\n"							\
            "        if (~CS && ~RW) begin\n"							\
            "            Mem[ADDR] <= DI;\n"							\
            "        end\n"												\
            "    end\n"													\
            "\n"														\
            "endmodule\n");
}

void output_binary(FILE *outf)
{
    unsigned int out_end = (tail_zero_start >= 0) ? (unsigned int)tail_zero_start : output_addr;
    for (unsigned int i = start_addr; i < out_end; i++) {
        fwrite(&output[i], 1, 1, outf);
    }
}

void calculate_chksum(void)
{
    unsigned short chksum = 0;
    for (unsigned int i = start_addr; i < output_addr; i += 2) {
        unsigned short tmp = (output[i + 1] << 8) | output[i];
        chksum += tmp;
    }
    chksum ^= 0xffff;
    output[chksum_addr    ] = chksum & 0xff;
    output[chksum_addr + 1] = chksum >> 8;
}

static char *get_error_string(int error)
{
    switch(error) {
    case NO_MEMORY_FOR_LABEL:
        return "No memory for labels";
    case CANNOT_RESOLVE_REF:
        return "Cannot resolve reference";
    case NO_MEMORY_FOR_MACRO:
        return "No memory for macro";
    case NO_MEMORY_FOR_PROC:
        return "No memory for proc";
    case INVALID_NUMBER:
        return "Invalid number";
    case INVALID_HEX_NUMBER:
        return "Invalid hex number";
    case INVALID_DECIMAL_NUMBER:
        return "Invalid decimal number";
    case INVALID_OCTAL_NUMBER:
        return "Invalid octal number";
    case INVALID_BINARY_NUMBER:
        return "Invalid binary number";
    case MISSED_BRACKET:
        return "Missed bracket";
    case EXPECTED_CLOSE_QUOTE:
        return "Expected close quote";
    case MISSED_OPCODE_PARAM_1:
        return "Missed parameter";
    case LONG_RELATED_OFFSET:
        return "Related offset too long";
    case MISSED_OPCODE_ARG_1:
        return "Missed argument 1";
    case EXPECTED_ARG_2:
        return "Expected argument 2";
    case MISSED_REGISTER_ARG_2:
        return "Missed register 2";
    case EXPECTED_ARG_3:
        return "Expected argument 3";
    case CONSTANT_VALUE_TOO_BIG:
        return "Constant value too big (> 16)";
    case MISSED_NAME_FOR_EQU:
        return "Missed name for equ";
    case MISSED_NAME_FOR_PROC:
        return "Missed name for procedure";
    case NESTED_PROC_UNSUPPORTED:
        return "Nested procedures are not supported";
    case ONLY_INSIDE_PROC:
        return "Only onside procedure";
    case LABEL_ALREADY_DEFINED:
        return "Label name already used";
    case MACRO_ALREADY_DEFINED:
        return "Macro name already used";
    case PROC_ALREADY_DEFINED:
        return "Procedure name already used";
    case EXTRA_SYMBOLS:
        return "Extra symbols";
    case SYNTAX_ERROR:
        return "Syntax error";
    case CANNOT_OPEN_FILE:
        return "Cannot open file";
    case UNSUPPORTED_INSTRUCTION:
        return "Unsupported instruction for CPU";
    default:
        return "No error";
    }
}

static char *get_out_name(char *in_str, char *ext)
{
    if (!in_str) {
        return NULL;
    }

    char *ptr = strrchr(in_str, '.');
    if (ptr) {
        *ptr = 0;
    }

    char *str = malloc(strlen(in_str) + strlen(ext) + 1);
    strcpy(str, in_str);
    strcat(str, ext);

    return str;
}

int main(int argc, char *argv[])
{
    int out_type = 0;
    char *input_path = NULL;
    char *output_path = NULL;
    char *list_path = NULL;
    const char *cpu_name = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-verilog|-binary] [--case-sensitive-symbols] [--jmp-label-indirect] [--cpu <name>] [--list <file|-] <input_file> [output_file]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-verilog")) {
            out_type = 1;
        } else if (!strcmp(argv[i], "-binary")) {
            out_type = 2;
        } else if (!strcmp(argv[i], "--case-sensitive-symbols")) {
            case_sensitive_symbols = 1;
        } else if (!strcmp(argv[i], "--jmp-label-indirect")) {
            jmp_label_indirect = 1;
        } else if (!strcmp(argv[i], "--cpu")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--cpu requires a name\n");
                return 1;
            }
            cpu_name = argv[++i];
        } else if (!strcmp(argv[i], "--list")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--list requires a file path\n");
                return 1;
            }
            list_path = argv[++i];
        } else if (!input_path) {
            input_path = argv[i];
        } else if (!output_path) {
            output_path = argv[i];
        } else {
            fprintf(stderr, "Too many arguments\n");
            return 1;
        }
    }

    if (!input_path) {
        fprintf(stderr, "Usage: %s [-verilog|-binary] [--case-sensitive-symbols] [--jmp-label-indirect] [--cpu <name>] [--list <file|-] <input_file> [output_file]\n", argv[0]);
        return 1;
    }

    if (cpu_name) {
        if (!set_cpu_by_name(cpu_name)) {
            fprintf(stderr, "Unknown CPU: %s\n", cpu_name);
            return 1;
        }
    }

    if (list_path) {
        if (!strcmp(list_path, "-")) {
            list_out = stdout;
        } else {
            list_out = fopen(list_path, "wb");
            if (!list_out) {
                fprintf(stderr, "Can't open listing file: %s\n", list_path);
                return 1;
            }
        }
    }

    start_addr = 0;

    in_file = fopen(input_path, "rb");
    if (in_file) {
        int err;
        char str[512];

        in_file_path = strdup(input_path);
        get_file_path(in_file_path);

        output_addr = start_addr;
        src_pass = 1;
        src_line = 1;
        in_macro = 0;
        in_proc = NULL;
        pad_tail_words = 0;
        emit_is_fill = 0;
        tail_zero_start = -1;

        // Pass 1

        do {
            if (files) {
                fclose(in_file);
                free(in_file_path);
                in_file = files->in_file;
                in_file_path = files->in_file_path;
                src_line = files->src_line;
                File *tmp = files->prev;
                free(files);
                files = tmp;
            }

            while(fgets(str, sizeof(str), in_file)) {
                char *ptr = str;
                REMOVE_ENDLINE(ptr);
                if ((err = do_asm(in_file, str)) || error != NO_ERROR) {
                    fprintf(stderr, "Line %d: %s\n", src_line, str);
                    fprintf(stderr, "Compilation failed: %s\n\n", get_error_string(error));
                    return 1;
                }
            }
        } while(files);

        output_addr = start_addr;
        src_pass = 2;
        src_line = 1;
        in_macro = 0;
        in_proc = NULL;
        emit_is_fill = 0;
        tail_zero_start = -1;

        if (fseek(in_file, 0, SEEK_SET) != 0) {
            fprintf(stderr, "Error rewinding file for pass 2\n");
            return 1;
        }

        // Pass 2

        do {
            if (files) {
                fclose(in_file);
                free(in_file_path);
                in_file = files->in_file;
                in_file_path = files->in_file_path;
                src_line = files->src_line;
                File *tmp = files->prev;
                free(files);
                files = tmp;
            }

            while(fgets(str, sizeof(str), in_file)) {
                char *ptr = str;
                REMOVE_ENDLINE(ptr);
                if ((err = do_asm(in_file, str)) || error != NO_ERROR) {
                    fprintf(stderr, "Line %d: %s\n", src_line, str);
                    fprintf(stderr, "Compilation failed: %s\n\n", get_error_string(error));
                    return 1;
                }
            }
        } while(files);

        if (use_chksum) {
            calculate_chksum();
        }

        if (pad_tail_words) {
            emit_word(0);
            emit_word(0);
        }

        if (list_out) {
            fprintf(list_out, "\nConstants:\n");
            dump_labels(equs);
            fprintf(list_out, "\nLabels:\n");
            dump_labels(labels);
            fprintf(list_out, "\nErrors: %s\n\n", get_error_string(error));
        }

        if (error == NO_ERROR) {
            char *name;
            if (output_path) {
                name = strdup(output_path);
            } else {
                name = get_out_name(input_path, (out_type == 2) ? ".bin" : (out_type == 1) ? ".v" : ".mem");
            }
            FILE *outf = fopen(name, "wb");
            if (outf) {
                if (out_type == 2) {
                    output_binary(outf);
                } else if (out_type) {
                    output_verilog(outf);
                } else {
                    output_hex(outf);
                }
                fclose(outf);
            } else {
                error = 1;
                fprintf(stderr, "Can't create output file!\n");
            }
            free(name);
        }

        fclose(in_file);
        free(in_file_path);
    } else {
        fprintf(stderr, "Cannot open input file!\n");
        return -1;
    }

    if (list_out && list_out != stdout) {
        fclose(list_out);
    }

    return error ? 1 : 0;
}
