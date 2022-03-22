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
    printf("[ ");
    Type_print(types, expr->ty);
    printf(" ]");

    if (expr->atomic) {
        if (!Type_is(&fun->types, expr->ty, fun->t_lexeme)) {
            printf(" ");

            // literals
            if (expr->ty.id == fun->t_string.id)
                printf(">>%.*s<<", (int)expr->string_.len, expr->string_.str);
            else if (expr->ty.id == fun->t_ident.id)
                printf("%.*s", (int)expr->ident.len, expr->ident.str);
            else if (expr->ty.id == fun->t_int.id)
                printf("%Ld", expr->int_);
            else if (expr->ty.id == fun->t_float.id)
                printf("%Lf", expr->float_);
            else if (expr->ty.id == fun->t_bool.id)
                printf("%s", expr->bool_ ? "true" : "false");
            else
                fungus_panic("unknown literal type!");
        }

        puts("");
    } else {
        // print child values
        const Word *name = Type_name(types, Rule_get(&fun->rules, expr->rule)->ty);

        printf(" %.*s\n", (int)name->len, name->str);

        for (size_t i = 0; i < expr->len; ++i)
            Expr_dump_r(fun, expr->exprs[i], level + 1);
    }
}

Expr **Expr_lhs(Expr *expr) {
    assert(!expr->atomic && expr->len > 0);

    return &expr->exprs[0];
}

Expr **Expr_rhs(Expr *expr) {
    assert(!expr->atomic && expr->len > 0);

    return &expr->exprs[expr->len - 1];
}

void Expr_dump(Fungus *fun, Expr *expr) {
    Expr_dump_r(fun, expr, 0);
}

void Expr_dump_array(Fungus *fun, Expr **exprs, size_t len) {
    for (size_t i = 0; i < len; ++i)
        Expr_dump(fun, exprs[i]);
}
