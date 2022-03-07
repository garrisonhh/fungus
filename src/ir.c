#include "ir.h"

char ITYPE_NAME[IT_COUNT][MAX_NAME_LEN];
char IOP_NAME[IOP_COUNT][MAX_NAME_LEN];
#define X(A, B) B,
const size_t IOP_NUM_OPERANDS[IOP_COUNT] = { IOP_TABLE };
#undef X

void ir_init(void) {
    // table names
#define X(A, B) #A,
    char *iop_names[IOP_COUNT] = { IOP_TABLE };
#undef X
#define X(A) #A,
    char *itype_names[IT_COUNT] = { ITYPE_TABLE };
#undef X

    names_to_lower(IOP_NAME, iop_names, IOP_COUNT);
    names_to_lower(ITYPE_NAME, itype_names, IT_COUNT);

#ifdef DEBUG
    // ensure IOP_MAX_OPERANDS is accurate
#define X(A, B) B,
    size_t iop_num_opers[IOP_COUNT] = { IOP_TABLE };
#undef X

    size_t true_max = 0;

    for (size_t i = 0; i < IOP_COUNT; ++i)
        if (iop_num_opers[i] > true_max)
            true_max = iop_num_opers[i];

    if (true_max != IOP_MAX_OPERANDS)
        fungus_panic("IOP_MAX_OPERANDS is %zu, should be %zu!",
                     IOP_MAX_OPERANDS, true_max);
#endif
}

IRContext IRContext_new(void) {
    IRContext ctx = {
        .pool = Bump_new(),
        .consts = Vec_new(),
        .funcs = Vec_new()
    };

    return ctx;
}

static void IFuncEntry_del(IFuncEntry *entry) {
    free(entry->ops);
}

void IRContext_del(IRContext *ctx) {
    for (size_t i = 0; i < ctx->funcs.len; ++i)
        IFuncEntry_del(ctx->funcs.data[i]);

    Vec_del(&ctx->funcs);
    Vec_del(&ctx->consts);
    Bump_del(&ctx->pool);
}

static void *IC_alloc(IRContext *ctx, size_t n_bytes) {
    return Bump_alloc(&ctx->pool, n_bytes);
}

static IFuncEntry *IC_get_func(IRContext *ctx, IFunc func) {
    return ctx->funcs.data[func.id];
}

#define IFUNC_INIT_CAP 4

IFunc ir_add_func(IRContext *ctx, Word name, IType *params, size_t num_params) {
    IFuncEntry *entry = IC_alloc(ctx, sizeof(*entry));
    IFunc handle = (IFunc){ ctx->funcs.len };

    *entry = (IFuncEntry){
        .name = Word_copy_of(&name, &ctx->pool),
        .ops = malloc(IFUNC_INIT_CAP * sizeof(*entry->ops)),
        .params = IC_alloc(ctx, num_params),
        .cap = IFUNC_INIT_CAP,
        .num_params = num_params
    };

    for (size_t i = 0; i < num_params; ++i)
        entry->params[i] = params[i];

    Vec_push(&ctx->funcs, entry);

    return handle;
}

size_t ir_add_op(IRContext *ctx, IFunc func, IOpType type, size_t *opers,
                   size_t num_opers) {
#ifdef DEBUG
    if (num_opers != IOP_NUM_OPERANDS[type]) {
        fungus_panic("attempted to create a %s IOp with %zu operands "
                     "(takes %zu)", IOP_NAME[type], num_opers,
                     IOP_NUM_OPERANDS[type]);
    }
#endif

    IFuncEntry *entry = IC_get_func(ctx, func);

    // allocate op
    if (entry->len == entry->cap) {
        entry->cap *= 2;
        entry->ops = realloc(entry->ops, entry->cap);
    }

    // fill
    IOp *op = &entry->ops[entry->len];

    op->type = type;

    for (size_t i = 0; i < num_opers; ++i)
        op->operands[i] = opers[i];

    return entry->num_params + entry->len++;
}

size_t ir_add_const(IRContext *ctx, IConst *iconst) {
    IConst *copy = IC_alloc(ctx, sizeof(*copy));
    size_t idx = ctx->consts.len;

    *copy = *iconst;

    Vec_push(&ctx->consts, copy);

    return idx;
}

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast) {
    fungus_panic("TODO ir_gen");
}

static void IConst_dump(IConst *iconst) {
    switch (iconst->type) {
    case IT_BOOL: printf("%s", iconst->bool_ ? "true" : "false"); break;
    case IT_I64: printf("%Ld", iconst->int_); break;
    case IT_F64: printf("%Lf", iconst->float_); break;
    case IT_COUNT: break;
    }

    puts("");
}

static void IOp_dump(IOp *op) {
    printf("%7s ", IOP_NAME[op->type]);

    for (size_t i = 0; i < IOP_NUM_OPERANDS[op->type]; ++i)
        printf("%4zu", op->operands[i]);

    puts("");
}

static void IFuncEntry_dump(IFuncEntry *entry) {
    printf("%.*s(", (int)entry->name->len, entry->name->str);

    for (size_t i = 0; i < entry->num_params; ++i) {
        if (i) printf(", ");
        printf("%zu %s", i, ITYPE_NAME[entry->params[i]]);
    }

    printf("):\n");

    for (size_t i = 0; i < entry->len; ++i) {
        printf("%3s%4zu ", "", i + entry->num_params);
        IOp_dump(&entry->ops[i]);
    }
}

void ir_dump(IRContext *ctx) {
    term_format(TERM_CYAN);
    puts("IR:");
    term_format(TERM_RESET);

    puts("consts:");

    for (size_t i = 0; i < ctx->consts.len; ++i) {
        printf("%3s%4zu ", "", i);

        IConst_dump(ctx->consts.data[i]);
    }

    puts("");

    for (size_t i = 0; i < ctx->funcs.len; ++i)
        IFuncEntry_dump(ctx->funcs.data[i]);

    puts("");
}

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx) {
    IFunc main = ir_add_func(ctx, WORD("main"), NULL, 0);

    IConst zero = (IConst){ .type = IT_I64, .int_ = 0 };

    size_t c0 = ir_add_const(ctx, &zero);

    size_t v0 = ir_add_op(ctx, main, IOP_CONST, &c0, 1);
    size_t v1 = ir_add_op(ctx, main, IOP_RET, &v0, 1);

    ir_dump(ctx);
}
#endif
