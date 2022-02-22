#include "expr.h"

/*
#define EXPR_INDENT 2

static void Expr_dump_r(Expr *expr, int level) {
    printf("%*s", level * EXPR_INDENT, "");

    // check validity
    if (!expr) {
        printf("(null)\n");
        return;
    }

    // print data
    TypeEntry *entry = type_get(expr->ty);

    printf("%.*s ", (int)entry->name->len, entry->name->str);

    switch (expr->mty.id) {
    case MTY_LITERAL:
        // print value
        switch (expr->ty.id) {
        case TY_STRING:
            printf(">>%.*s<<", (int)expr->_string.len, expr->_string.str);
            break;
        case TY_INT:
            printf("%ld", expr->_int);
            break;
        case TY_FLOAT:
            printf("%lf", expr->_float);
            break;
        case TY_BOOL:
            printf("%s", expr->_bool ? "true" : "false");
            break;
        }

        printf("\n");

        break;
    default:
        // print metatype name
        entry = metatype_get(expr->mty);

        printf("%.*s\n", (int)entry->name->len, entry->name->str);

        // print child values
        for (size_t i = 0; i < expr->len; ++i)
            Expr_dump_r(expr->exprs[i], level + 1);

        break;
    }
}
*/

void Expr_dump(TypeGraph *tg, Expr *expr) {
    puts("TODO expr dump");

    // Expr_dump_r(expr, 0);
}

