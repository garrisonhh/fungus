#include <ctype.h>

#include "fungus.h"
#include "base_fungus.h"
#include "lex.h"

// sorts by descending length
static int lexeme_cmp(const void *a, const void *b) {
    return (int)(*(Word **)b)->len - (int)(*(Word **)a)->len;
}

static Type define_lexeme(Fungus *fun, Word name) {
    TypeDef lexeme_def = { name, &fun->t_lexeme, 1 };

    return Type_define(&fun->types, &lexeme_def);
}

Type Fungus_define_symbol(Fungus *fun, Word symbol) {
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

    return define_lexeme(fun, symbol);

invalid_symbol:
    fungus_error(">>%.*s<< is not a valid symbol.",
                 (int)symbol.len, symbol.str);
    global_error = true;

    return INVALID_TYPE;
}

Type Fungus_define_keyword(Fungus *fun, Word keyword) {
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

    return define_lexeme(fun, keyword);

invalid_keyword:
    fungus_panic(">>%.*s<< is not a valid keyword.",
                 (int)keyword.len, keyword.str);
    global_error = true;

    return INVALID_TYPE;
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

    if (children) {
        expr->len = children;
        expr->exprs = Fungus_tmp_alloc(fun, expr->len * sizeof(*expr->exprs));
    }

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

// ensure ty is not a comptype and cty is
static void ensure_valid_types(Fungus *fun, Type ty, Type cty) {
    bool actual_ty = !Type_is(&fun->types, ty, fun->t_comptype);
    bool actual_mty = Type_is(&fun->types, cty, fun->t_comptype);

    if (actual_ty && actual_mty)
        return;

    printf("l%s%sks fishy ><> ", actual_ty ? "o" : "O", actual_mty ? "o" : "O");
}

void Fungus_print_types(Fungus *fun, Type ty, Type cty) {
    ensure_valid_types(fun, ty, cty);

    const Word *ty_name = Type_name(&fun->types, ty);
    const Word *mty_name = Type_name(&fun->types, cty);

    printf("[ %.*s | %.*s ]",
           (int)ty_name->len, ty_name->str,
           (int)mty_name->len, mty_name->str);
}

size_t Fungus_sprint_types(Fungus *fun, char *str, Type ty, Type cty) {
    ensure_valid_types(fun, ty, cty);

    const Word *ty_name = Type_name(&fun->types, ty);
    const Word *mty_name = Type_name(&fun->types, cty);

    return sprintf(str, "[ %.*s | %.*s ]",
                   (int)ty_name->len, ty_name->str,
                   (int)mty_name->len, mty_name->str);
}
