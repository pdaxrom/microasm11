#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum {
    INPUT_AUTO = 0,
    INPUT_BINARY,
    INPUT_OBJECT,
};

enum {
    RELOC_LSB = 0x01,
    RELOC_MSB = 0x02,
    RELOC_WORD = 0x03,
    RELOC_PCREL_WORD = 0x04,
};

typedef struct {
    char name[16];
    unsigned int value;
    int absolute;
} Symbol;

typedef struct {
    unsigned char type;
    unsigned int value;
    unsigned int offset;
    int external;
} Reloc;

typedef struct {
    Symbol *entries;
    unsigned int entry_count;
    Symbol *externs;
    unsigned int extern_count;
    Reloc *relocs;
    unsigned int reloc_count;
    unsigned char *code;
    unsigned int code_len;
    unsigned int code_file_offset;
    unsigned int entry_offset;
} Object;

typedef struct {
    unsigned char *code;
    unsigned int code_len;
    unsigned int off;
    unsigned int used;
    unsigned int addr;
    int truncated;
} DecodeCtx;

static const char *regs[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "sp", "pc"
};

static unsigned int origin = 0;

static unsigned int read_u16(unsigned char *buf)
{
    return buf[0] | (buf[1] << 8);
}

static unsigned int read_u32(unsigned char *buf)
{
    return read_u16(buf) | (read_u16(buf + 2) << 16);
}

static int parse_number(char *str, unsigned int *value)
{
    int base = 10;
    char *end;

    if (*str == '$') {
        base = 16;
        str++;
    } else if (*str == '%') {
        unsigned int val = 0;

        str++;
        if (*str != '0' && *str != '1') {
            return 1;
        }
        while (*str == '0' || *str == '1' || *str == '_') {
            if (*str != '_') {
                val = (val << 1) | (*str - '0');
            }
            str++;
        }
        if (*str) {
            return 1;
        }
        *value = val;
        return 0;
    } else if (str[0] == '0' && tolower((unsigned char)str[1]) == 'x') {
        base = 16;
    }

    *value = strtoul(str, &end, base);
    return *end != 0;
}

static int read_file(char *path, unsigned char **buf, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    long len;

    if (!fp) {
        fprintf(stderr, "Cannot open input file: %s\n", path);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return 1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 1;
    }

    *buf = malloc(len ? (size_t)len : 1);
    if (!*buf) {
        fclose(fp);
        fprintf(stderr, "Out of memory\n");
        return 1;
    }
    if (fread(*buf, 1, len, fp) != (size_t)len) {
        free(*buf);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    *size = len;
    return 0;
}

static void read_name(char *dst, unsigned char *src)
{
    memcpy(dst, src, 15);
    dst[15] = 0;
}

static int looks_like_object(unsigned char *buf, size_t size)
{
    unsigned int ent_count;
    unsigned int ext_count;
    unsigned int code_len;
    unsigned int code_offset;

    if (size < 0x20 || read_u16(buf) != 0x5aa5 || read_u16(buf + 2) != 1) {
        return 0;
    }

    ent_count = read_u16(buf + 4);
    ext_count = read_u16(buf + 6);
    code_len = read_u16(buf + 8);
    code_offset = read_u32(buf + 0x0a);

    return code_offset >= 0x20 + (ent_count + ext_count) * 20 &&
           code_offset <= size &&
           code_len <= size - code_offset;
}

static int parse_object(unsigned char *buf, size_t size, Object *obj)
{
    unsigned int ent_count = read_u16(buf + 4);
    unsigned int ext_count = read_u16(buf + 6);
    unsigned int code_len = read_u16(buf + 8);
    unsigned int code_offset = read_u32(buf + 0x0a);
    unsigned int pos;

    if (!looks_like_object(buf, size)) {
        fprintf(stderr, "Invalid object file\n");
        return 1;
    }

    memset(obj, 0, sizeof(*obj));
    obj->entry_count = ent_count;
    obj->extern_count = ext_count;
    obj->code_len = code_len;
    obj->code_file_offset = code_offset;
    obj->entry_offset = read_u16(buf + 0x0e);

    obj->entries = calloc(ent_count ? ent_count : 1, sizeof(Symbol));
    obj->externs = calloc(ext_count ? ext_count : 1, sizeof(Symbol));
    obj->code = malloc(code_len ? code_len : 1);
    if (!obj->entries || !obj->externs || !obj->code) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    pos = 0x20;
    for (unsigned int i = 0; i < ent_count; i++) {
        read_name(obj->entries[i].name, buf + pos);
        obj->entries[i].value = read_u16(buf + pos + 0x10);
        obj->entries[i].absolute = (short)read_u16(buf + pos + 0x12) < 0;
        pos += 20;
    }
    for (unsigned int i = 0; i < ext_count; i++) {
        read_name(obj->externs[i].name, buf + pos);
        pos += 20;
    }

    memcpy(obj->code, buf + code_offset, code_len);
    pos = code_offset + code_len;
    if (pos + 2 > size) {
        fprintf(stderr, "Missing relocation table\n");
        return 1;
    }

    obj->reloc_count = read_u16(buf + pos);
    pos += 2;
    if (pos + obj->reloc_count * 5 > size) {
        fprintf(stderr, "Invalid relocation table\n");
        return 1;
    }

    obj->relocs = calloc(obj->reloc_count ? obj->reloc_count : 1,
                         sizeof(Reloc));
    if (!obj->relocs) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    for (unsigned int i = 0; i < obj->reloc_count; i++) {
        unsigned char raw_type = buf[pos++];

        obj->relocs[i].external = (raw_type & 0x80) != 0;
        obj->relocs[i].type = raw_type & 0x7f;
        obj->relocs[i].value = read_u16(buf + pos);
        obj->relocs[i].offset = read_u16(buf + pos + 2);
        pos += 4;
    }

    return 0;
}

static void free_object(Object *obj)
{
    free(obj->entries);
    free(obj->externs);
    free(obj->relocs);
    free(obj->code);
}

static unsigned int sext8(unsigned int val)
{
    return (val & 0x80) ? (val | 0xffffff00) : val;
}

static unsigned int sext16(unsigned int val)
{
    return (val & 0x8000) ? (val | 0xffff0000) : val;
}

static unsigned int fetch_ext(DecodeCtx *ctx)
{
    unsigned int pos = ctx->off + ctx->used;

    if (pos + 1 >= ctx->code_len) {
        ctx->truncated = 1;
        return 0;
    }
    ctx->used += 2;
    return read_u16(ctx->code + pos);
}

static void fmt_octal(char *buf, size_t size, unsigned int val)
{
    snprintf(buf, size, "%06o", val & 0xffff);
}

static void format_ea(DecodeCtx *ctx, unsigned int spec, char *buf,
                      size_t size)
{
    unsigned int mode = (spec >> 3) & 7;
    unsigned int reg = spec & 7;
    unsigned int ext;
    unsigned int target;
    char tmp[32];

    switch (mode) {
    case 0:
        snprintf(buf, size, "%s", regs[reg]);
        break;
    case 1:
        snprintf(buf, size, "(%s)", regs[reg]);
        break;
    case 2:
        if (reg == 7) {
            ext = fetch_ext(ctx);
            fmt_octal(tmp, sizeof(tmp), ext);
            snprintf(buf, size, "#%s", tmp);
        } else {
            snprintf(buf, size, "(%s)+", regs[reg]);
        }
        break;
    case 3:
        if (reg == 7) {
            ext = fetch_ext(ctx);
            fmt_octal(tmp, sizeof(tmp), ext);
            snprintf(buf, size, "@#%s", tmp);
        } else {
            snprintf(buf, size, "@(%s)+", regs[reg]);
        }
        break;
    case 4:
        snprintf(buf, size, "-(%s)", regs[reg]);
        break;
    case 5:
        snprintf(buf, size, "@-(%s)", regs[reg]);
        break;
    case 6:
        ext = fetch_ext(ctx);
        if (reg == 7) {
            target = (ctx->addr + ctx->used + (int)sext16(ext)) & 0xffff;
            fmt_octal(tmp, sizeof(tmp), target);
            snprintf(buf, size, "%s", tmp);
        } else {
            fmt_octal(tmp, sizeof(tmp), ext);
            snprintf(buf, size, "%s(%s)", tmp, regs[reg]);
        }
        break;
    case 7:
        ext = fetch_ext(ctx);
        if (reg == 7) {
            target = (ctx->addr + ctx->used + (int)sext16(ext)) & 0xffff;
            fmt_octal(tmp, sizeof(tmp), target);
            snprintf(buf, size, "@%s", tmp);
        } else {
            fmt_octal(tmp, sizeof(tmp), ext);
            snprintf(buf, size, "@%s(%s)", tmp, regs[reg]);
        }
        break;
    default:
        snprintf(buf, size, "???");
        break;
    }
}

static void format_ccode(unsigned int word, char *buf, size_t size)
{
    static const char *clear_names[] = { "clc", "clv", "clz", "cln" };
    static const char *set_names[] = { "sec", "sev", "sez", "sen" };
    const char **names = (word & 0000020) ? set_names : clear_names;
    unsigned int bits = word & 017;
    int first = 1;

    if (word == 0000240) {
        snprintf(buf, size, "nop");
        return;
    }
    if (word == 0000257) {
        snprintf(buf, size, "ccc");
        return;
    }
    if (word == 0000277) {
        snprintf(buf, size, "scc");
        return;
    }

    buf[0] = 0;
    for (int i = 0; i < 4; i++) {
        if (bits & (1u << i)) {
            if (!first) {
                strncat(buf, "|", size - strlen(buf) - 1);
            }
            strncat(buf, names[i], size - strlen(buf) - 1);
            first = 0;
        }
    }
    if (first) {
        snprintf(buf, size, "dw %06o", word);
    }
}

static int decode_exact(unsigned int word, char *buf, size_t size)
{
    struct Exact {
        unsigned int word;
        const char *name;
    };
    static const struct Exact exacts[] = {
        { 0000000, "halt" },
        { 0000001, "wait" },
        { 0000002, "rti" },
        { 0000003, "bpt" },
        { 0000004, "iot" },
        { 0000005, "reset" },
        { 0000006, "rtt" },
        { 0000007, "mfpt" },
        { 0000012, "go" },
        { 0000016, "step" },
        { 0000020, "rsel" },
        { 0000021, "mfus" },
        { 0000022, "rcpc" },
        { 0000024, "rcps" },
        { 0000031, "mtus" },
        { 0000032, "wcpc" },
        { 0000034, "wcps" },
        { 0170000, "cfcc" },
        { 0170001, "setf" },
        { 0170002, "seti" },
        { 0170011, "setd" },
        { 0170012, "setl" },
    };

    for (unsigned int i = 0; i < sizeof(exacts) / sizeof(exacts[0]); i++) {
        if (word == exacts[i].word) {
            snprintf(buf, size, "%s", exacts[i].name);
            return 1;
        }
    }
    return 0;
}

static int decode_branch(unsigned int word, unsigned int addr, char *buf,
                         size_t size)
{
    struct Branch {
        unsigned int base;
        const char *name;
    };
    static const struct Branch branches[] = {
        { 0000400, "br" },
        { 0001000, "bne" },
        { 0001400, "beq" },
        { 0002000, "bge" },
        { 0002400, "blt" },
        { 0003000, "bgt" },
        { 0003400, "ble" },
        { 0100000, "bpl" },
        { 0100400, "bmi" },
        { 0101000, "bhi" },
        { 0101400, "blos" },
        { 0102000, "bvc" },
        { 0102400, "bvs" },
        { 0103000, "bcc" },
        { 0103400, "bcs" },
    };

    for (unsigned int i = 0; i < sizeof(branches) / sizeof(branches[0]); i++) {
        if ((word & 0177400) == branches[i].base) {
            unsigned int target = (addr + 2 + ((int)sext8(word & 0377) << 1)) &
                                  0xffff;
            char tmp[32];

            fmt_octal(tmp, sizeof(tmp), target);
            snprintf(buf, size, "%s %s", branches[i].name, tmp);
            return 1;
        }
    }
    return 0;
}

static int decode_double(DecodeCtx *ctx, unsigned int word, char *buf,
                         size_t size)
{
    static const char *normal_names[] = {
        NULL, "mov", "cmp", "bit", "bic", "bis", "add"
    };
    static const char *byte_names[] = {
        NULL, "movb", "cmpb", "bitb", "bicb", "bisb"
    };
    unsigned int op = (word >> 12) & 017;
    const char *name = NULL;
    char src[64];
    char dst[64];

    if (op >= 1 && op <= 6) {
        name = normal_names[op];
    } else if (op >= 011 && op <= 015) {
        name = byte_names[op - 010];
    } else if (op == 016) {
        name = "sub";
    }

    if (!name) {
        return 0;
    }

    format_ea(ctx, (word >> 6) & 077, src, sizeof(src));
    format_ea(ctx, word & 077, dst, sizeof(dst));
    snprintf(buf, size, "%s %s, %s", name, src, dst);
    return 1;
}

static int decode_single(DecodeCtx *ctx, unsigned int word, char *buf,
                         size_t size)
{
    struct Single {
        unsigned int base;
        const char *name;
        int allow_byte;
    };
    static const struct Single singles[] = {
        { 0000300, "swab", 0 },
        { 0005000, "clr", 1 },
        { 0005100, "com", 1 },
        { 0005200, "inc", 1 },
        { 0005300, "dec", 1 },
        { 0005400, "neg", 1 },
        { 0005500, "adc", 1 },
        { 0005600, "sbc", 1 },
        { 0005700, "tst", 1 },
        { 0006000, "ror", 1 },
        { 0006100, "rol", 1 },
        { 0006200, "asr", 1 },
        { 0006300, "asl", 1 },
        { 0006500, "mfpi", 0 },
        { 0006600, "mtpi", 0 },
        { 0006700, "sxt", 0 },
        { 0007000, "csm", 0 },
        { 0007200, "tstset", 0 },
        { 0007300, "wrtlck", 0 },
        { 0106400, "mtps", 0 },
        { 0106500, "mfpd", 0 },
        { 0106600, "mtpd", 0 },
        { 0106700, "mfps", 0 },
    };
    int is_byte = (word & 0100000) != 0;
    unsigned int base = word & 0177700;
    char ea[64];

    for (unsigned int i = 0; i < sizeof(singles) / sizeof(singles[0]); i++) {
        if (base == singles[i].base) {
            format_ea(ctx, word & 077, ea, sizeof(ea));
            snprintf(buf, size, "%s %s", singles[i].name, ea);
            return 1;
        }
    }

    if (is_byte) {
        base = word & 0077700;
        for (unsigned int i = 0; i < sizeof(singles) / sizeof(singles[0]); i++) {
            if (base == singles[i].base && singles[i].allow_byte) {
                format_ea(ctx, word & 077, ea, sizeof(ea));
                snprintf(buf, size, "%sb %s", singles[i].name, ea);
                return 1;
            }
        }
    }

    return 0;
}

static int decode_fp(DecodeCtx *ctx, unsigned int word, char *buf,
                     size_t size)
{
    struct FpEa {
        unsigned int base;
        const char *name;
    };
    static const struct FpEa f4[] = {
        { 0170100, "ldfps" },
        { 0170200, "stfps" },
        { 0170300, "stst" },
    };
    static const struct FpEa f2[] = {
        { 0170400, "clrf" },
        { 0170500, "tstf" },
        { 0170600, "absf" },
        { 0170700, "negf" },
    };
    static const struct FpEa f13[] = {
        { 0171000, "mulf" },
        { 0171400, "modf" },
        { 0172000, "addf" },
        { 0172400, "ldf" },
        { 0173000, "subf" },
        { 0173400, "cmpf" },
        { 0174000, "stf" },
        { 0174400, "divf" },
        { 0175000, "stexp" },
        { 0175400, "stcfi" },
        { 0176000, "stcfd" },
        { 0176400, "ldexp" },
        { 0177000, "ldcif" },
        { 0177400, "ldcdf" },
    };
    unsigned int base;
    unsigned int ac = (word >> 6) & 3;
    char ea[64];

    base = word & 0177700;
    for (unsigned int i = 0; i < sizeof(f4) / sizeof(f4[0]); i++) {
        if (base == f4[i].base) {
            format_ea(ctx, word & 077, ea, sizeof(ea));
            snprintf(buf, size, "%s %s", f4[i].name, ea);
            return 1;
        }
    }
    for (unsigned int i = 0; i < sizeof(f2) / sizeof(f2[0]); i++) {
        if (base == f2[i].base) {
            format_ea(ctx, word & 077, ea, sizeof(ea));
            snprintf(buf, size, "%s %s", f2[i].name, ea);
            return 1;
        }
    }

    base = word & 0177400;
    for (unsigned int i = 0; i < sizeof(f13) / sizeof(f13[0]); i++) {
        if (base == f13[i].base) {
            int ac_first = !strcmp(f13[i].name, "stf") ||
                           !strcmp(f13[i].name, "stcfd") ||
                           !strcmp(f13[i].name, "stexp") ||
                           !strcmp(f13[i].name, "stcfi");

            format_ea(ctx, word & 077, ea, sizeof(ea));
            if (ac_first) {
                snprintf(buf, size, "%s ac%u, %s", f13[i].name, ac, ea);
            } else {
                snprintf(buf, size, "%s %s, ac%u", f13[i].name, ea, ac);
            }
            return 1;
        }
    }

    return 0;
}

static void decode_instruction(unsigned int off, unsigned char *code,
                               unsigned int code_len, char *buf, size_t size,
                               unsigned int *used)
{
    DecodeCtx ctx;
    unsigned int word;
    char tmp1[64];

    memset(&ctx, 0, sizeof(ctx));
    ctx.code = code;
    ctx.code_len = code_len;
    ctx.off = off;
    ctx.used = 2;
    ctx.addr = (origin + off) & 0xffff;

    word = read_u16(code + off);

    if (decode_exact(word, buf, size)) {
        goto done;
    }
    if (word >= 0000240 && word <= 0000277) {
        format_ccode(word, buf, size);
        goto done;
    }
    if ((word & 0177770) == 0000230) {
        snprintf(buf, size, "spl %o", word & 7);
        goto done;
    }
    if ((word & 0177700) == 0000100) {
        format_ea(&ctx, word & 077, tmp1, sizeof(tmp1));
        snprintf(buf, size, "jmp %s", tmp1);
        goto done;
    }
    if ((word & 0177000) == 0004000) {
        format_ea(&ctx, word & 077, tmp1, sizeof(tmp1));
        snprintf(buf, size, "jsr %s, %s", regs[(word >> 6) & 7], tmp1);
        goto done;
    }
    if ((word & 0177770) == 0000200) {
        snprintf(buf, size, "rts %s", regs[word & 7]);
        goto done;
    }
    if ((word & 0177700) == 0006400) {
        snprintf(buf, size, "mark %o", word & 077);
        goto done;
    }
    if ((word & 0177000) == 0077000) {
        unsigned int target = (ctx.addr + 2 - ((word & 077) << 1)) & 0xffff;

        fmt_octal(tmp1, sizeof(tmp1), target);
        snprintf(buf, size, "sob %s, %s", regs[(word >> 6) & 7], tmp1);
        goto done;
    }
    if (decode_branch(word, ctx.addr, buf, size)) {
        goto done;
    }
    if ((word & 0177400) == 0104000 || (word & 0177400) == 0104400) {
        snprintf(buf, size, "%s %03o",
                 ((word & 0177400) == 0104000) ? "emt" : "trap",
                 word & 0377);
        goto done;
    }
    if ((word & 0177000) >= 0070000 && (word & 0177000) <= 0073000) {
        static const char *names[] = { "mul", "div", "ash", "ashc" };
        unsigned int idx = ((word & 0177000) - 0070000) >> 9;

        format_ea(&ctx, word & 077, tmp1, sizeof(tmp1));
        snprintf(buf, size, "%s %s, %s", names[idx], tmp1,
                 regs[(word >> 6) & 7]);
        goto done;
    }
    if ((word & 0177000) == 0074000) {
        format_ea(&ctx, word & 077, tmp1, sizeof(tmp1));
        snprintf(buf, size, "xor %s, %s", regs[(word >> 6) & 7], tmp1);
        goto done;
    }
    if ((word & 0177770) >= 0075000 && (word & 0177770) <= 0075030 &&
            (((word & 0177770) - 0075000) % 010) == 0) {
        static const char *names[] = { "fadd", "fsub", "fmul", "fdiv" };
        unsigned int idx = ((word & 0177770) - 0075000) >> 3;

        snprintf(buf, size, "%s %s", names[idx], regs[word & 7]);
        goto done;
    }
    if ((word & 0170000) == 0170000 && decode_fp(&ctx, word, buf, size)) {
        goto done;
    }
    if (decode_single(&ctx, word, buf, size)) {
        goto done;
    }
    if (decode_double(&ctx, word, buf, size)) {
        goto done;
    }

    snprintf(buf, size, "dw %06o", word);

done:
    if (ctx.truncated) {
        strncat(buf, " ; truncated", size - strlen(buf) - 1);
    }
    *used = ctx.used;
}

static void print_matching_labels(Object *obj, unsigned int offset)
{
    for (unsigned int i = 0; i < obj->entry_count; i++) {
        if (!obj->entries[i].absolute && obj->entries[i].value == offset) {
            printf("%s:\n", obj->entries[i].name);
        }
    }
}

static const char *reloc_kind(unsigned int type)
{
    switch (type) {
    case RELOC_WORD:
        return "word";
    case RELOC_LSB:
        return "lsb";
    case RELOC_MSB:
        return "msb";
    case RELOC_PCREL_WORD:
        return "pcrel-word";
    default:
        return "?";
    }
}

static void append_reloc_notes(char *dst, size_t size, Object *obj,
                               unsigned int off, unsigned int used)
{
    for (unsigned int i = 0; i < obj->reloc_count; i++) {
        Reloc *reloc = &obj->relocs[i];
        char sym[32];

        if (reloc->offset < off || reloc->offset >= off + used) {
            continue;
        }

        if (reloc->external && reloc->value < obj->extern_count) {
            snprintf(sym, sizeof(sym), "%s", obj->externs[reloc->value].name);
        } else {
            snprintf(sym, sizeof(sym), "code_base");
        }

        if (dst[0]) {
            strncat(dst, ", ", size - strlen(dst) - 1);
        }
        snprintf(dst + strlen(dst), size - strlen(dst), "reloc %s %s",
                 reloc_kind(reloc->type), sym);
    }
}

static void format_bytes(unsigned char *code, unsigned int off,
                         unsigned int code_len, unsigned int used, char *buf,
                         size_t size)
{
    buf[0] = 0;
    for (unsigned int i = 0; i < used && off + i < code_len; i += 2) {
        char tmp[16];

        if (i > 0) {
            strncat(buf, " ", size - strlen(buf) - 1);
        }
        if (off + i + 1 < code_len) {
            snprintf(tmp, sizeof(tmp), "%06o", read_u16(code + off + i));
        } else {
            snprintf(tmp, sizeof(tmp), "%03o", code[off + i]);
        }
        strncat(buf, tmp, size - strlen(buf) - 1);
    }
}

static void disassemble(unsigned char *code, unsigned int code_len,
                        Object *obj)
{
    unsigned int off = 0;

    while (off < code_len) {
        unsigned int addr = (origin + off) & 0xffff;
        unsigned int used = 1;
        char inst[128];
        char bytes[96];
        char note[256] = "";

        if (obj) {
            print_matching_labels(obj, off);
        }

        if (off + 1 >= code_len) {
            snprintf(inst, sizeof(inst), "db %03o", code[off]);
        } else {
            decode_instruction(off, code, code_len, inst, sizeof(inst), &used);
            if (used == 0) {
                used = 2;
            }
        }
        if (used > code_len - off) {
            used = code_len - off;
        }
        format_bytes(code, off, code_len, used, bytes, sizeof(bytes));

        if (obj) {
            append_reloc_notes(note, sizeof(note), obj, off, used);
        }

        printf("%06o: %-21s %s", addr, bytes, inst);
        if (note[0]) {
            printf(" ; %s", note);
        }
        printf("\n");

        off += used;
    }
}

static void print_object_header(Object *obj)
{
    printf("; object file\n");
    for (unsigned int i = 0; i < obj->extern_count; i++) {
        printf("extern %s\n", obj->externs[i].name);
    }
    for (unsigned int i = 0; i < obj->entry_count; i++) {
        printf("public %s\n", obj->entries[i].name);
    }
    for (unsigned int i = 0; i < obj->entry_count; i++) {
        if (obj->entries[i].absolute) {
            printf("%s equ %06o\n", obj->entries[i].name,
                   obj->entries[i].value & 0xffff);
        }
    }
    if (obj->entry_offset != 0xffff &&
            obj->entry_offset >= obj->code_file_offset &&
            obj->entry_offset < obj->code_file_offset + obj->code_len) {
        printf("; entry %06o\n",
               (origin + obj->entry_offset - obj->code_file_offset) & 0xffff);
    } else {
        printf("; entry none\n");
    }
    printf("\n");
}

static void print_usage(char *prog)
{
    fprintf(stderr,
            "Usage: %s [-binary|-object|-obj] [-org address] <input.bin|input.obj>\n",
            prog);
}

int main(int argc, char *argv[])
{
    int input_type = INPUT_AUTO;
    char *input_name = NULL;
    unsigned char *buf = NULL;
    size_t size = 0;
    int ret = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-binary")) {
            input_type = INPUT_BINARY;
        } else if (!strcmp(argv[i], "-object") || !strcmp(argv[i], "-obj")) {
            input_type = INPUT_OBJECT;
        } else if (!strcmp(argv[i], "-org")) {
            if (++i >= argc || parse_number(argv[i], &origin)) {
                print_usage(argv[0]);
                return 1;
            }
        } else if (!input_name) {
            input_name = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input_name) {
        print_usage(argv[0]);
        return 1;
    }

    if (read_file(input_name, &buf, &size)) {
        return 1;
    }

    if (input_type == INPUT_AUTO) {
        input_type = looks_like_object(buf, size) ? INPUT_OBJECT :
                     INPUT_BINARY;
    }

    if (input_type == INPUT_OBJECT) {
        Object obj;

        if (parse_object(buf, size, &obj)) {
            goto done;
        }
        print_object_header(&obj);
        disassemble(obj.code, obj.code_len, &obj);
        free_object(&obj);
    } else {
        disassemble(buf, size, NULL);
    }

    ret = 0;

done:
    free(buf);
    return ret;
}
