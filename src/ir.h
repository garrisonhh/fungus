#ifndef IR_H
#define IR_H

#include "fungus.h"

typedef enum IType {
    IT_BOOL,
    IT_INT,
    IT_FLOAT
} IType;

typedef enum IOpType {
    IOP_ADD_I,
    IOP_SUB_I,
    IOP_MUL_I,
    IOP_DIV_I,
    IOP_MOD_I,
} IOpType;

typedef struct IOp {
    IOpType type;
    size_t a, b; // operand function indices
} IOp;

typedef struct IFunc {
    IOp *ops;
    size_t len, cap;
} IFunc;

typedef struct IRContext {
    Bump pool;
    Vec funcs;

    IFunc *main;
} IRContext;

// takes in contexts and expr, returns a function index
typedef size_t (*IRHook)(Fungus *fun, IRContext *ctx, IFunc *func, Expr *expr);

IRContext IRContext_new(void);
void IRContext_del(IRContext *);

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast);

#endif
