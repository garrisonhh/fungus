#include "ir.h"

char ITYPE_NAME[ITYPE_COUNT][MAX_NAME_LEN];
char IINST_NAME[II_COUNT][MAX_NAME_LEN];
char IINST_TYPE_NAME[IIT_COUNT][MAX_NAME_LEN];

#define X(A, B) B,
IInstType IINST_TYPE[II_COUNT] = { IINST_TABLE };
#undef X

void ir_init(void) {
    // table names
#define X(A) #A,
    char *itype_names[ITYPE_COUNT] = { ITYPE_TABLE };
#undef X
#define X(A, B) #A,
    char *iinst_names[II_COUNT] = { IINST_TABLE };
#undef X
#define X(A) #A,
    char *iinst_type_names[IIT_COUNT] = { IINST_TYPE_TABLE };
#undef X

    names_to_lower(ITYPE_NAME, itype_names, ITYPE_COUNT);
    names_to_lower(IINST_NAME, iinst_names, II_COUNT);
    names_to_lower(IINST_TYPE_NAME, iinst_type_names, IIT_COUNT);
}

IRContext IRContext_new(void) {
    IRContext ctx = {
        .pool = Bump_new(),
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

size_t ir_add_op(IRContext *ctx, IFunc func, IOp op) {
    IFuncEntry *entry = IC_get_func(ctx, func);

    // allocate op
    if (entry->len == entry->cap) {
        entry->cap *= 2;
        entry->ops = realloc(entry->ops, entry->cap);
    }

    // fill
    IOp *copy = &entry->ops[entry->len];

    *copy = op;

    return entry->num_params + entry->len++;
}

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast) {
    fungus_panic("TODO ir_gen");
}

static void IOp_dump(IRContext *ctx, IOp *op) {
    printf("%s ", IINST_NAME[op->inst]);

    switch (IINST_TYPE[op->inst]) {
    case IIT_NIL:
    case IIT_COUNT:
        break;
    case IIT_CONST: {
        IConst *iconst = &op->iconst;

        printf("%s ", ITYPE_NAME[iconst->type]);

        switch (iconst->type) {
        case ITYPE_BOOL: printf("`%s`", iconst->bool_ ? "true" : "false"); break;
        case ITYPE_I64: printf("`%Ld`", iconst->int_); break;
        case ITYPE_F64: printf("`%Lf`", iconst->float_); break;
        case ITYPE_COUNT: break;
        }
        break;
    }
    case IIT_UNARY:
        printf("%zu", op->unary.a);
        break;
    case IIT_BINARY:
        printf("%zu %zu", op->binary.a, op->binary.b);
        break;
    case IIT_CALL: {
        IFuncEntry *entry = IC_get_func(ctx, op->call.func);

        printf("%.*s(", (int)entry->name->len, entry->name->str);

        for (size_t i = 0; i < op->call.num_params; ++i) {
            if (i) printf(", ");
            printf("%zu", op->call.params[i]);
        }

        printf(")");
        break;
    }
    }

    puts("");
}

static void IFuncEntry_dump(IRContext *ctx, IFuncEntry *entry) {
    printf("%.*s(", (int)entry->name->len, entry->name->str);

    for (size_t i = 0; i < entry->num_params; ++i) {
        if (i) printf(", ");
        printf("%zu %s", i, ITYPE_NAME[entry->params[i]]);
    }

    printf("):\n");

    for (size_t i = 0; i < entry->len; ++i) {
        printf("%4s$%zu = ", "", i + entry->num_params);
        IOp_dump(ctx, &entry->ops[i]);
    }
}

void ir_dump(IRContext *ctx) {
    term_format(TERM_CYAN);
    puts("IR:");
    term_format(TERM_RESET);

    for (size_t i = 0; i < ctx->funcs.len; ++i)
        IFuncEntry_dump(ctx, ctx->funcs.data[i]);

    puts("");
}

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx) {
    IFunc main = ir_add_func(ctx, WORD("main"), NULL, 0);

    size_t v0 = ir_add_op(ctx, main, (IOp){
        .inst = II_CONST,
        .iconst = { ITYPE_I64, .int_ = 0 },
    });

    size_t v1 = ir_add_op(ctx, main, (IOp){
        .inst = II_RET,
        .unary = { v0 }
    });

    ir_dump(ctx);
}
#endif
