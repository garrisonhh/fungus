#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "lex.h"
#include "fungus.h"
#include "data.h"
#include "utils.h"

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

static Expr *RawExprBuf_next(RawExprBuf *rebuf, Type ty, Type cty) {
    Expr *expr = RawExprBuf_alloc(rebuf);

    expr->ty = ty;
    expr->cty = cty;

    return expr;
}

// lexing ======================================================================

Lexer Lexer_new(void) {
    return (Lexer){
        .pool = Bump_new(),
        .symbols = Vec_new(),
        .keywords = Vec_new()
    };
}

void Lexer_del(Lexer *lex) {
    Vec_del(&lex->keywords);
    Vec_del(&lex->symbols);
    Bump_del(&lex->pool);
}

static void dump_word_vec(Vec *vec) {
    for (size_t i = 0; i < vec->len; ++i) {
        Word *word = vec->data[i];

        printf("%.*s ", (int)word->len, word->str);
    }
}

void Lexer_dump(Lexer *lex) {
    term_format(TERM_CYAN);
    puts("Lexer:");
    term_format(TERM_RESET);

    printf("symbols: ");
    dump_word_vec(&lex->symbols);
    printf("\nkeywords: ");
    dump_word_vec(&lex->keywords);
    printf("\n\n");
}

static bool is_wordable(char c) {
    return c == '_' || isalnum(c);
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

static bool match_string(Fungus *fun, RawExprBuf *rebuf, LZip *z) {
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
        Expr *expr = RawExprBuf_next(rebuf, fun->t_string, fun->t_literal);

        expr->string_ = parse_string(&v);

        return true;
    }

    return false;
}

static bool match_int(Fungus *fun, RawExprBuf *rebuf, LZip *zip) {
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
    Expr *expr = RawExprBuf_next(rebuf, fun->t_int, fun->t_literal);

    expr->int_ = num;

    return true;
}

static bool match_float(Fungus *fun, RawExprBuf *rebuf, LZip *zip) {
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
    Expr *expr = RawExprBuf_next(rebuf, fun->t_float, fun->t_literal);

    expr->float_ = num;

    return true;
}

static bool match_symbol(Fungus *fun, RawExprBuf *rebuf, LZip *zip) {
    Lexer *lex = &fun->lexer;
    size_t remaining = zip->len - zip->idx;

    for (size_t i = 0; i < lex->symbols.len; ++i) {
        Word *sym = lex->symbols.data[i];

        if (remaining >= sym->len
            && !strncmp(zip->str + zip->idx, sym->str, sym->len)) {
            zip->idx += sym->len;

            Type type = Type_get(&fun->types, sym);

            if (type.id == INVALID_TYPE.id)
                fungus_panic("matched invalid symbol type");

            RawExprBuf_next(rebuf, fun->t_notype, type);

            return true;
        }
    }

    return false;
}

static bool match_keyword(Fungus *fun, RawExprBuf *rebuf, LZip *zip) {
    Lexer *lex = &fun->lexer;
    size_t remaining = zip->len - zip->idx;

    for (size_t i = 0; i < lex->keywords.len; ++i) {
        Word *kw = lex->keywords.data[i];

        if (remaining >= kw->len
            && !strncmp(zip->str + zip->idx, kw->str, kw->len)
            && !is_wordable(zip->str[zip->idx + kw->len])) {
            zip->idx += kw->len;

            Type type = Type_get(&fun->types, kw);

            if (type.id == INVALID_TYPE.id)
                fungus_panic("matched invalid keyword type");

            RawExprBuf_next(rebuf, fun->t_notype, type);

            return true;
        }
    }

    return false;
}

static bool err_no_match(Fungus *fun, RawExprBuf *rebuf, LZip *zip) {
    (void)fun;
    (void)rebuf;

    fungus_error("failed to tokenize at:");
    // TODO better error reporting here?
    printf(">>%.*s<<\n", 60, zip->str + zip->idx);
    global_error = true;

    return false;
}

RawExprBuf tokenize(Fungus *fun, const char *str, size_t len) {
    RawExprBuf rebuf = RawExprBuf_new();
    LZip zip = LZip_new(str, len);

    bool (*match_funcs[])(Fungus *, RawExprBuf *, LZip *) = {
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
            if (match_funcs[i](fun, &rebuf, &zip))
                break;

        if (global_error)
            goto err_exit;
    }

    return rebuf;

err_exit:
    RawExprBuf_del(&rebuf);

    return (RawExprBuf){0};
}

void RawExprBuf_dump(Fungus *fun, RawExprBuf *rebuf) {
    term_format(TERM_CYAN);
    puts("RawExprBuf:");
    term_format(TERM_RESET);

    for (size_t i = 0; i < rebuf->len; ++i)
        Expr_dump(fun, &rebuf->exprs[i]);

    puts("");
}
