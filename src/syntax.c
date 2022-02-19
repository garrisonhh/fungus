#include <string.h>

#include "syntax.h"
#include "precedence.h"

RuleTree ruletree;

void syntax_init(void) {
    ;
}

void syntax_quit(void) {
    ;
}

// parsing =====================================================================

IterCtx IterCtx_new(void) {
    return (IterCtx){
        .pool = Bump_new()
    };
}

void IterCtx_del(IterCtx *ctx) {
    Bump_del(&ctx->pool);
}

static Expr *alloc_expr(IterCtx *ctx, Type ty, MetaType mty) {
    Expr *expr = Bump_alloc(&ctx->pool, sizeof(*expr));

    expr->ty = ty;
    expr->mty = mty;

    return expr;
}

Expr *syntax_parse(IterCtx *ctx, Expr *exprs, size_t len) {
    Expr **slice = malloc(len * sizeof(*exprs));

    for (size_t i = 0; i < len; ++i)
        slice[i] = &exprs[i];

    fungus_panic("parsing with RuleTrees is not implemented atm.");
}
