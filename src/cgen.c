#include "cgen.h"

const char *C_BEGIN_CODE =
"#include <stdbool.h>\n"
"#include <stdint.h>\n"
;

// map itypes to c types
#define X(A, B) B,
static const char *C_TYPE_OF[ITYPE_COUNT] = { ITYPE_TABLE };
#undef X

// for printf-ing IOps
#define X(A, B, C) C,
static const char *C_FMT_OF[II_COUNT] = { IINST_TABLE };
#undef X

static void gen_op(FILE *to, Fungus *fun, size_t n, IOp *op) {
    IInstType inst_type = IINST_TYPE[op->inst];

    if (inst_type == IIT_NOP) {
        ;
    } else if (inst_type == IIT_RET) {
        fprintf(to, "return v%zu", op->a);
    } else {
        // intermediate var
        fprintf(to, "%s v%zu = ", C_TYPE_OF[op->ty], n);

        // operands
        switch (inst_type) {
        case IIT_CONST:
            ir_fprint_const(to, op);
            break;
        case IIT_UNARY:
            fprintf(to, C_FMT_OF[op->inst], op->a);
            break;
        case IIT_BINARY:
            fprintf(to, C_FMT_OF[op->inst], op->a, op->b);
            break;
        default:
            fungus_panic("unexpected inst type `%s` in gen_op",
                         IINST_TYPE_NAME[IINST_TYPE[op->inst]]);
        }
    }
}

static void gen_func(FILE *to, Fungus *fun, IFuncEntry *entry) {
    // params
    // TODO IFunc type inference
    fprintf(to, "%s %.*s(", C_TYPE_OF[entry->ret_ty], (int)entry->name->len,
            entry->name->str);

    for (size_t i = 0; i < entry->num_params; ++i) {
        if (i) fprintf(to, ", ");
        fprintf(to, "%s v%zu", C_TYPE_OF[entry->params[i]], i);
    }

    fprintf(to, ") {\n");

    // operations
    for (size_t i = 0; i < entry->len; ++i) {
        fprintf(to, "%4s", "");
        gen_op(to, fun, i + entry->num_params, &entry->ops[i]);
        fprintf(to, ";\n");
    }

    fprintf(to, "}\n");
}

void c_gen(FILE *to, Fungus *fun, IRContext *ctx) {
    fprintf(to, "%s", C_BEGIN_CODE);

    for (size_t i = 0; i < ctx->funcs.len; ++i)
        gen_func(to, fun, ctx->funcs.data[i]);
}
