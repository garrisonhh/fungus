#include "cgen.h"

const char *C_BEGIN_CODE =
"#include <stdbool.h>\n"
"#include <stdint.h>\n"
;

// map itypes to c types
const char *CTYPE_OF[ITYPE_COUNT] = {
    [ITYPE_NIL] = "void",
    [ITYPE_BOOL] = "bool",
    [ITYPE_I64] = "int64_t",
    [ITYPE_F64] = "double",
};

// used by simpler iinst types to output c
const char *OP_FMT[II_COUNT] = {
    [II_ADD] = "v%zu + v%zu",
    [II_SUB] = "v%zu - v%zu",
    [II_MUL] = "v%zu * v%zu",
    [II_DIV] = "v%zu / v%zu",
    [II_MOD] = "v%zu % v%zu",
};

static void gen_op(FILE *to, Fungus *fun, size_t n, IOp *op) {
    IInstType inst_type = IINST_TYPE[op->inst];

    if (inst_type == IIT_NOP) {
        ;
    } else if (inst_type == IIT_RET) {
        fprintf(to, "return v%zu", op->a);
    } else {
        // intermediate var
        fprintf(to, "%s v%zu = ", CTYPE_OF[op->ty], n);

        // operands
        switch (inst_type) {
        case IIT_CONST:
            ir_fprint_const(to, op);
            break;
        case IIT_UNARY:
            fprintf(to, OP_FMT[op->inst], op->a);
            break;
        case IIT_BINARY:
            fprintf(to, OP_FMT[op->inst], op->a, op->b);
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
    fprintf(to, "%s %.*s(", CTYPE_OF[entry->ret_ty], (int)entry->name->len,
            entry->name->str);

    for (size_t i = 0; i < entry->num_params; ++i) {
        if (i) fprintf(to, ", ");
        fprintf(to, "%s v%zu", CTYPE_OF[entry->params[i]], i);
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
