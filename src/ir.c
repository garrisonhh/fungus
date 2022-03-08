#include "ir.h"

char ITYPE_NAME[ITYPE_COUNT][MAX_NAME_LEN];
char IINST_NAME[II_COUNT][MAX_NAME_LEN];
char IINST_TYPE_NAME[IIT_COUNT][MAX_NAME_LEN];

#define X(A, B, C) B,
IInstType IINST_TYPE[II_COUNT] = { IINST_TABLE };
#undef X

void ir_init(void) {
    // table names
#define X(A, B) #A,
    char *itype_names[ITYPE_COUNT] = { ITYPE_TABLE };
#undef X
#define X(A, B, C) #A,
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

IFuncEntry *ir_get_func(IRContext *ctx, IFunc func) {
    return ctx->funcs.data[func.id];
}

#define IFUNC_INIT_CAP 4

IFunc ir_add_func(IRContext *ctx, IType ret_ty, Word name, IType *params,
                  size_t num_params) {
    IFuncEntry *entry = IC_alloc(ctx, sizeof(*entry));
    IFunc handle = (IFunc){ ctx->funcs.len };

    *entry = (IFuncEntry){
        .name = Word_copy_of(&name, &ctx->pool),
        .ops = malloc(IFUNC_INIT_CAP * sizeof(*entry->ops)),
        .params = IC_alloc(ctx, num_params),
        .cap = IFUNC_INIT_CAP,
        .num_params = num_params,
        .ret_ty = ret_ty
    };

    for (size_t i = 0; i < num_params; ++i)
        entry->params[i] = params[i];

    Vec_push(&ctx->funcs, entry);

    return handle;
}

static IType itype_of(IFuncEntry *entry, size_t op_addr) {
    if (op_addr < entry->num_params)
        return entry->params[op_addr];
    else
        return entry->ops[op_addr - entry->num_params].ty;
}

static void ir_infer_op_type(IRContext *ctx, IFunc func, IOp *op) {
    // verify op
    if (op->inst == II_CONST) {
        if (op->ty == ITYPE_MUST_INFER)
            fungus_panic("const op itypes must be provided.");

        return;
    } else if (op->ty != ITYPE_MUST_INFER) {
        fungus_panic("non-const op itypes are inferred.");
    }

    // infer type
    IFuncEntry *entry = ir_get_func(ctx, func);

    switch (IINST_TYPE[op->inst]) {
    case IIT_COUNT:
    case IIT_CONST:
        fungus_panic("unreachable");
        break;
    case IIT_NOP:
    case IIT_RET:
        op->ty = ITYPE_NIL;
        break;
    case IIT_UNARY:
        op->ty = itype_of(entry, op->a);
        break;
    case IIT_BINARY: {
        IType ty_a = itype_of(entry, op->a);

#ifdef DEBUG
        IType ty_b = itype_of(entry, op->b);

        if (ty_a != ty_b)
            fungus_panic("binary op itypes do not match!");
#endif

        op->ty = ty_a;
        break;
    }
    case IIT_CALL: {
        IFuncEntry *call_entry = ir_get_func(ctx, op->call.func);

        op->ty = call_entry->ret_ty;
        break;
    }
    }
}

size_t ir_add_op(IRContext *ctx, IFunc func, IOp op) {
    IFuncEntry *entry = ir_get_func(ctx, func);

    // allocate op
    if (entry->len >= entry->cap) {
        entry->cap *= 2;
        entry->ops = realloc(entry->ops, entry->cap * sizeof(*entry->ops));
    }

    // fill
    IOp *copy = &entry->ops[entry->len];

    *copy = op;

    // copy function call params
    if (copy->inst == II_CALL) {
        copy->call.params =
            IC_alloc(ctx, copy->call.num_params * sizeof(*copy->call.params));

        for (size_t i = 0; i < copy->call.num_params; ++i)
            copy->call.params[i] = op.call.params[i];
    }

    ir_infer_op_type(ctx, func, copy);

    return entry->num_params + entry->len++;
}

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast) {
    fungus_panic("TODO ir_gen");
}

void ir_fprint_const(FILE *fp, IOp *op) {
    switch (op->ty) {
    case ITYPE_BOOL: fprintf(fp, "%s", op->bool_ ? "true" : "false"); break;
    case ITYPE_I64: fprintf(fp, "%Ld", op->int_); break;
    case ITYPE_F64: fprintf(fp, "%Lf", op->float_); break;
    default: break;
    }
}

static void IOp_dump(IRContext *ctx, IOp *op) {
    printf("%s ", IINST_NAME[op->inst]);

    switch (IINST_TYPE[op->inst]) {
    case IIT_NOP:
    case IIT_COUNT:
        break;
    case IIT_CONST:
        printf("%s ", ITYPE_NAME[op->ty]);
        ir_fprint_const(stdout, op);
        break;
    case IIT_RET:
    case IIT_UNARY:
        printf("$%zu", op->a);
        break;
    case IIT_BINARY:
        printf("$%zu $%zu", op->a, op->b);
        break;
    case IIT_CALL: {
        IFuncEntry *entry = ir_get_func(ctx, op->call.func);

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
    printf("%s %.*s(", ITYPE_NAME[entry->ret_ty], (int)entry->name->len,
           entry->name->str);

    for (size_t i = 0; i < entry->num_params; ++i) {
        if (i) printf(", ");
        printf("%zu %s", i, ITYPE_NAME[entry->params[i]]);
    }

    printf("):\n");

    for (size_t i = 0; i < entry->len; ++i) {
        IOp *op = &entry->ops[i];

        printf("%4s", "");

        // some ops return data, some don't
        if (op->ty != ITYPE_NIL)
            printf("$%zu = ", i + entry->num_params);

        IOp_dump(ctx, op);
    }
}

void ir_dump(IRContext *ctx) {
    term_format(TERM_CYAN);
    puts("IR:");
    term_format(TERM_RESET);

    for (size_t i = 0; i < ctx->funcs.len; ++i) {
        if (i) puts("");
        IFuncEntry_dump(ctx, ctx->funcs.data[i]);
    }

    puts("");
}

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx) {
    IFunc add = ir_add_func(ctx, ITYPE_I64, WORD("add"),
                            (IType[]){ ITYPE_I64, ITYPE_I64 }, 2);
    {
        size_t v0 = ir_add_op(ctx, add, (IOp){
            II_ADD, .a = 0, .b = 1
        });
        ir_add_op(ctx, add, (IOp){ II_RET, .a = v0 });
    }

    IFunc main = ir_add_func(ctx, ITYPE_I64, WORD("main"), NULL, 0);
    {
        size_t v0 = ir_add_op(ctx, main, (IOp){
            II_CONST, ITYPE_I64, .int_ = 1
        });
        size_t v1 = ir_add_op(ctx, main, (IOp){
            II_CONST, ITYPE_I64, .int_ = 2
        });
        size_t v2 = ir_add_op(ctx, main, (IOp){ II_CALL, .call = {
            .func = add, .params = (size_t[]){ v0, v1 }, .num_params = 2
        }});
        ir_add_op(ctx, main, (IOp){ II_RET, .a = v2 });
    }

    ir_dump(ctx);
}
#endif
