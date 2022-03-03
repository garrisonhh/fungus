#include <assert.h>

#include "expr.h"
#include "fungus.h"

#define EXPR_INDENT 2

static void Expr_dump_r(Fungus *fun, Expr *expr, int level) {
    printf("%*s", level * EXPR_INDENT, "");

    // check validity
    if (!expr) {
        printf("(null expr)\n");
        return;
    }

    // print data
    Fungus_print_types(fun, expr->ty, expr->cty);
    printf(" ");

    if (expr->cty.id == fun->t_literal.id) {
        // literals
        if (expr->ty.id == fun->t_string.id)
            printf(">>%.*s<<", (int)expr->string_.len, expr->string_.str);
        else if (expr->ty.id == fun->t_int.id)
            printf("%ld", expr->int_);
        else if (expr->ty.id == fun->t_float.id)
            printf("%lf", expr->float_);
        else if (expr->ty.id == fun->t_bool.id)
            printf("%s", expr->bool_ ? "true" : "false");
        else
            fungus_panic("unknown literal type!");

        puts("");
    } else if (Type_is(&fun->types, expr->cty, fun->t_lexeme)) {
        // lexemes
        puts("raw lexeme");
    } else {
        // print child values
        puts("");

        for (size_t i = 0; i < expr->len; ++i)
            Expr_dump_r(fun, expr->exprs[i], level + 1);
    }
}

void Expr_dump(Fungus *fun, Expr *expr) {
    Expr_dump_r(fun, expr, 0);
}

void Expr_dump_array(Fungus *fun, Expr **exprs, size_t len) {
    for (size_t i = 0; i < len; ++i)
        Expr_dump(fun, exprs[i]);
}
