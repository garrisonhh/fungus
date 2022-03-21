#include <assert.h>

#include "expr.h"
#include "fungus.h"

#define EXPR_INDENT 2

static void Expr_dump_r(Fungus *fun, Expr *expr, int level) {
    TypeGraph *types = &fun->types;

    printf("%*s", level * EXPR_INDENT, "");

    // check validity
    if (!expr) {
        printf("(null expr)\n");
        return;
    }

    // print data
    TypeExpr_print(types, expr->te);

    if (TypeExpr_matches(&fun->types, expr->te, fun->te_prim_literal)) {
        Type prim = expr->te->exprs[0]->atom;

        printf(" ");

        // literals
        if (prim.id == fun->t_string.id)
            printf(">>%.*s<<", (int)expr->string_.len, expr->string_.str);
        else if (prim.id == fun->t_int.id)
            printf("%Ld", expr->int_);
        else if (prim.id == fun->t_float.id)
            printf("%Lf", expr->float_);
        else if (prim.id == fun->t_bool.id)
            printf("%s", expr->bool_ ? "true" : "false");
        else
            fungus_panic("unknown literal type!");

        puts("");
    } else if (TypeExpr_is(types, expr->te, fun->t_lexeme)) {
        // lexemes
        ;
    } else if (TypeExpr_matches(types, expr->te, fun->te_rule)) {
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
