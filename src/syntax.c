#include "syntax.h"

IterCtx IterCtx_new(void) {
    return (IterCtx){
        .pool = Bump_new()
    };
}

void IterCtx_del(IterCtx *ctx) {
    Bump_del(&ctx->pool);
}

Expr *parse(Fungus *fun, IterCtx *ctx, Expr *exprs, size_t len) {
    puts("TODO PARSE");

    return NULL;
}
