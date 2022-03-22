#include <ctype.h>
#include <assert.h>

#include "fungus.h"
#include "base_fungus.h"
#include "lex.h"

// sorts by descending length
static int lexeme_cmp(const void *a, const void *b) {
    return (int)(*(Word **)b)->len - (int)(*(Word **)a)->len;
}

static Type define_lexeme(Fungus *fun, Word name) {
    return Type_define(&fun->types, &(TypeDef){
        .name = name,
        .is = &fun->t_lexeme,
        .is_len = 1
    });
}

bool Fungus_define_symbol(Fungus *fun, Word symbol, Type *o_type) {
    assert(o_type);

    // validate symbol
    if (symbol.len == 0)
        goto invalid_symbol;

    for (size_t i = 0; i < symbol.len; ++i)
        if (!ispunct(symbol.str[i]))
            goto invalid_symbol;

    // register symbol
    Word *copy = Word_copy_of(&symbol, &fun->lexer.pool);

    Vec_push(&fun->lexer.symbols, copy);
    Vec_qsort(&fun->lexer.symbols, lexeme_cmp); // TODO this is inefficient af

    *o_type = define_lexeme(fun, symbol);

    return true;

invalid_symbol:
    fungus_error(">>%.*s<< is not a valid symbol.",
                 (int)symbol.len, symbol.str);
    global_error = true;

    return false;
}

bool Fungus_define_keyword(Fungus *fun, Word keyword, Type *o_type) {
    assert(o_type);

    // validate keyword
    if (keyword.len == 0 || !isalpha(keyword.str[0]))
        goto invalid_keyword;

    for (size_t i = 1; i < keyword.len; ++i)
        if (!(isalpha(keyword.str[i]) || keyword.str[i] == '_'))
            goto invalid_keyword;

    // register keyword
    Word *copy = Word_copy_of(&keyword, &fun->lexer.pool);

    Vec_push(&fun->lexer.keywords, copy);
    Vec_qsort(&fun->lexer.keywords, lexeme_cmp); // TODO this is inefficient af

    *o_type = define_lexeme(fun, keyword);

    return true;

invalid_keyword:
    fungus_panic(">>%.*s<< is not a valid keyword.",
                 (int)keyword.len, keyword.str);
    global_error = true;

    return false;
}

Type Fungus_base_symbol(Fungus *fun, Word symbol) {
    Type ty;

    if (Fungus_define_symbol(fun, symbol, &ty))
        return ty;

    fungus_panic("failed to define base symbol: `%.*s`",
                 (int)symbol.len, symbol.str);
}

Type Fungus_base_keyword(Fungus *fun, Word keyword) {
    Type ty;

    if (Fungus_define_keyword(fun, keyword, &ty))
        return ty;

    fungus_panic("failed to define base keyword: %.*s",
                 (int)keyword.len, keyword.str);
}

Fungus Fungus_new(void) {
    Fungus fun = {
        .lexer = Lexer_new(),
        .types = TypeGraph_new(),
        .precedences = PrecGraph_new(),
        .rules = RuleTree_new(),

        .temp = Bump_new()
    };

    Fungus_define_base(&fun);

#if 1
    Fungus_dump(&fun);
#endif

    return fun;
}

void Fungus_del(Fungus *fun) {
    Bump_del(&fun->temp);

    RuleTree_del(&fun->rules);
    PrecGraph_del(&fun->precedences);
    TypeGraph_del(&fun->types);
    Lexer_del(&fun->lexer);
}

void *Fungus_tmp_alloc(Fungus *fun, size_t n_bytes) {
    return Bump_alloc(&fun->temp, n_bytes);
}

void Fungus_tmp_clear(Fungus *fun) {
    Bump_clear(&fun->temp);
}

Expr *Fungus_tmp_expr(Fungus *fun, size_t children) {
    Expr *expr = Fungus_tmp_alloc(fun, sizeof(*expr));

    expr->len = children;

    if (expr->len)
        expr->exprs = Fungus_tmp_alloc(fun, expr->len * sizeof(*expr->exprs));

    return expr;
}

void Fungus_dump(Fungus *fun) {
    term_format(TERM_YELLOW);
    puts("Fungus Definitions");
    term_format(TERM_RESET);

    Lexer_dump(&fun->lexer);
    TypeGraph_dump(&fun->types);
    PrecGraph_dump(&fun->precedences);
    RuleTree_dump(fun, &fun->rules);
}
