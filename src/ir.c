#include "ir.h"

static void *IC_alloc(IRContext *ctx, size_t n_bytes) {
    return Bump_alloc(&ctx->pool, n_bytes);
}

static IFunc *IFunc_new(IRContext *ctx) {
    IFunc *func = IC_alloc(ctx, sizeof(*func));

    Vec_push(&ctx->funcs, func);

    return func;
}

IRContext IRContext_new(void) {
    IRContext ctx = {
        .pool = Bump_new(),
        .funcs = Vec_new()
    };

    ctx.main = IFunc_new(&ctx);

    return ctx;
}

void IRContext_del(IRContext *ctx) {
    Vec_del(&ctx->funcs);
    Bump_del(&ctx->pool);
}

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast) {
    // TODO
}
