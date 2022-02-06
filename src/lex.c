#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "lex.h"
#include "data.h"
#include "utils.h"
#include "types.h"

// LZip ========================================================================

typedef struct LexZipper {
    const char *str;
    size_t idx, len;
} LZip;

static LZip LZip_new(const char *str, size_t len) {
    return (LZip){ .str = str, .len = len };
}

static char LZip_peek(LZip *z) {
    if (z->idx < z->len)
        return z->str[z->idx];

    fungus_panic("lzip overextended.\n");
}

static char LZip_next(LZip *z) {
    char c = LZip_peek(z);

    ++z->idx;

    return c;
}

static bool LZip_done(LZip *z) {
    return z->idx >= z->len;
}

// RawExprBuf ==================================================================

#define REBUF_INIT_CAP 32

static RawExprBuf RawExprBuf_new(void) {
    return (RawExprBuf){
        .exprs = malloc(REBUF_INIT_CAP * sizeof(Expr)),
        .cap = REBUF_INIT_CAP
    };
}

void RawExprBuf_del(RawExprBuf *rebuf) {
    free(rebuf->exprs);
}

static Expr *RawExprBuf_alloc(RawExprBuf *rebuf) {
    if (rebuf->len + 1 >= rebuf->cap) {
        rebuf->cap *= 2;
        rebuf->exprs = realloc(rebuf->exprs,
                                rebuf->cap * sizeof(*rebuf->exprs));
    }

    return &rebuf->exprs[rebuf->len++];
}

static Expr *RawExprBuf_next(RawExprBuf *rebuf, Type ty, MetaType mty) {
    Expr *expr = RawExprBuf_alloc(rebuf);

    expr->ty = ty;
    expr->mty = mty;

    return expr;
}

// lexing ======================================================================

// TODO unit lookups are dumb stupid just-get-it-working design, have so much
// easy optimization potential
typedef struct LexTable {
    Bump pool;
    Vec symbols, keywords;
} LexTable;

static LexTable lex_tab;

void lex_init(void) {
    lex_tab.pool = Bump_new();
    lex_tab.symbols = Vec_new();
    lex_tab.keywords = Vec_new();
}

void lex_quit(void) {
    Vec_del(&lex_tab.keywords);
    Vec_del(&lex_tab.symbols);
    Bump_del(&lex_tab.pool);
}

static bool is_wordable(char c) {
    return c == '_' || isalnum(c);
}

// sorts by descending length
static int lex_unit_cmp(const void *a, const void *b) {
    return (int)(*(Word **)b)->len - (int)(*(Word **)a)->len;
}

MetaType lex_def_symbol(Word *symbol) {
    // validate symbol
    if (symbol->len == 0)
        goto invalid_symbol;

    for (size_t i = 0; i < symbol->len; ++i)
        if (!ispunct(symbol->str[i]))
            goto invalid_symbol;

    // register symbol
    Word *copy = Word_copy_of(symbol, &lex_tab.pool);

    Vec_push(&lex_tab.symbols, copy);
    Vec_qsort(&lex_tab.symbols, lex_unit_cmp); // TODO this is inefficient af

    return metatype_define(copy);

invalid_symbol:
    fungus_error(">>%.*s<< is not a valid symbol.",
                 (int)symbol->len, symbol->str);
    global_error = true;

    return METATYPE(0);
}

MetaType lex_def_keyword(Word *keyword) {
    // validate keyword
    if (keyword->len == 0 || !is_wordable(keyword->str[0]))
        goto invalid_keyword;

    for (size_t i = 1; i < keyword->len; ++i)
        if (!is_wordable(keyword->str[i]))
            goto invalid_keyword;

    // register keyword
    Word *copy = Word_copy_of(keyword, &lex_tab.pool);

    Vec_push(&lex_tab.keywords, copy);
    Vec_qsort(&lex_tab.keywords, lex_unit_cmp); // TODO this is inefficient af

    return metatype_define(keyword);

invalid_keyword:
    fungus_panic(">>%.*s<< is not a valid keyword.",
                 (int)keyword->len, keyword->str);
    global_error = true;

    return METATYPE(0);
}

static void skip_whitespace(LZip *z) {
    while (isspace(z->str[z->idx]))
        ++z->idx;
}

static Word parse_string(View *lit) {
    char *str = malloc((lit->len - 2) * sizeof(*str)); // TODO this is leaked
    size_t len = 0;

    for (size_t i = 1; i < lit->len - 1; ++i) {
        const char c = lit->str[i], prev = lit->str[i - 1];

        if (prev == '\\') {
            switch (c) {
#define CASE(A, B) case A: str[len++] = B; break
            CASE('\\', '\\');
            CASE('n', '\n');
            CASE('t', '\t');
#undef CASE
            case '\n': break;
            default: str[len++] = c; continue;
            }
        } else if (c != '\\') {
            str[len++] = c;
        }
    }

    return Word_new(str, len);
}

static bool match_string(RawExprBuf *rebuf, LZip *z) {
    char c = LZip_peek(z);

    if (c == '"') {
        // find string length
        char prev;
        View v = { .str = &z->str[z->idx], .len = 1 };

        LZip_next(z);

        do {
            ++v.len;

            prev = c;
            c = LZip_next(z);
        } while (!(c == '"' && prev != '\\'));

        // emit token
        Expr *expr = RawExprBuf_next(rebuf, TYPE(TY_STRING),
                                     METATYPE(MTY_LITERAL));

        expr->_string = parse_string(&v);

        return true;
    }

    return false;
}

static bool match_int(RawExprBuf *rebuf, LZip *zip) {
    // strtol wrangling
    const char *start = zip->str + zip->idx;
    char *end;
    long num = strtol(start, &end, 10);

    if (start == end || *end == '.')
        return false;

    zip->idx += end - start;

    switch (errno) {
    case EINVAL:
        return false;
    case ERANGE:
        fungus_error("integer out of range: %.*s", (int)(end - start), start);
        global_error = true;

        return false;
    }

    // token
    Expr *expr = RawExprBuf_next(rebuf, TYPE(TY_INT), METATYPE(MTY_LITERAL));

    expr->_int = num;

    return true;
}

static bool match_float(RawExprBuf *rebuf, LZip *zip) {
    // strtod wrangling
    const char *start = zip->str + zip->idx;
    char *end;
    double num = strtod(start, &end);

    if (start == end)
        return false;

    zip->idx += end - start;

    if (errno == ERANGE) {
        fungus_error("float out of range: %.*s", (int)(end - start), start);
        global_error = true;

        return false;
    }

    // token
    Expr *expr = RawExprBuf_next(rebuf, TYPE(TY_FLOAT), METATYPE(MTY_LITERAL));

    expr->_float = num;

    return true;
}

static bool match_symbol(RawExprBuf *rebuf, LZip *zip) {
    size_t remaining = zip->len - zip->idx;

    for (size_t i = 0; i < lex_tab.symbols.len; ++i) {
        Word *sym = lex_tab.symbols.data[i];

        if (remaining >= sym->len
            && !strncmp(zip->str + zip->idx, sym->str, sym->len)) {
            TypeEntry *entry = metatype_get_by_name(sym);
            MetaType mty = METATYPE(entry->id);

            if (!entry)
                mty = metatype_define(sym);

            RawExprBuf_next(rebuf, TYPE(TY_NONE), mty);
            zip->idx += sym->len;

            return true;
        }
    }

    return false;
}

static bool match_keyword(RawExprBuf *rebuf, LZip *zip) {
    size_t remaining = zip->len - zip->idx;

    for (size_t i = 0; i < lex_tab.keywords.len; ++i) {
        Word *kw = lex_tab.keywords.data[i];

        if (remaining >= kw->len
            && !strncmp(zip->str + zip->idx, kw->str, kw->len)
            && !is_wordable(zip->str[zip->idx + kw->len])) {
            TypeEntry *entry = metatype_get_by_name(kw);
            MetaType mty = METATYPE(entry->id);

            if (!entry)
                mty = metatype_define(kw);

            RawExprBuf_next(rebuf, TYPE(TY_NONE), mty);
            zip->idx += kw->len;

            return true;
        }
    }

    return false;
}

static bool err_no_match(RawExprBuf *rebuf, LZip *zip) {
    (void)rebuf;

    fungus_error("failed to tokenize at:");
    // TODO better error reporting here?
    printf(">>%.*s<<\n", 60, zip->str + zip->idx);
    global_error = true;

    return false;
}

RawExprBuf tokenize(const char *str, size_t len) {
    RawExprBuf rebuf = RawExprBuf_new();
    LZip zip = LZip_new(str, len);

    bool (*match_funcs[])(RawExprBuf *, LZip *) = {
        match_symbol,
        match_keyword,
        match_string,
        match_int,
        match_float,
        err_no_match
    };

    while (!LZip_done(&zip)) {
        skip_whitespace(&zip);

        for (size_t i = 0; i < ARRAY_SIZE(match_funcs); ++i)
            if (match_funcs[i](&rebuf, &zip))
                break;

        if (global_error)
            goto err_exit;
    }

#if 1
    term_format(TERM_YELLOW);
    printf("    %-16s%-16s\n", "type", "metatype");
    term_format(TERM_RESET);

    for (size_t i = 0; i < rebuf.len; ++i) {
        Expr *expr = &rebuf.exprs[i];
        Word *ty_name = type_get(expr->ty)->name;
        Word *mty_name = metatype_get(expr->mty)->name;

        printf("    %-16.*s%-16.*s\n", (int)ty_name->len, ty_name->str,
               (int)mty_name->len, mty_name->str);
    }

    printf("\n");
#endif

    return rebuf;

err_exit:
    RawExprBuf_del(&rebuf);

    return (RawExprBuf){0};
}
