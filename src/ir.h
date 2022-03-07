#ifndef IR_H
#define IR_H

#include "fungus.h"

/*
 * ir contains only sparse type info; types can be extracted from c
 */

#define ITYPE_TABLE\
    X(BOOL)\
    X(I64)\
    X(F64)\

#define X(A) IT_##A,
typedef enum IType { ITYPE_TABLE IT_COUNT } IType;
#undef X

typedef struct IConst {
    IType type;
    union {
        fun_bool bool_;
        fun_int int_;
        fun_float float_;
    };
} IConst;

extern char ITYPE_NAME[IT_COUNT][MAX_NAME_LEN];

/*
 * descriptions of not perfectly clear IOPs:
 * const - gives a local name to an IConst in ctx->constants
 */

// table of (iop, num operands)
#define IOP_TABLE\
    /* special */\
    X(NOP,      0)\
    X(RET,      1)\
    X(CONST,    1)\
    /* math */\
    X(ADD,      2)\
    X(SUB,      2)\
    X(MUL,      2)\
    X(DIV,      2)\
    X(MOD,      2)\

#define IOP_MAX_OPERANDS 2

#define X(A, B) IOP_##A,
typedef enum IOpType { IOP_TABLE IOP_COUNT } IOpType;
#undef X

extern char IOP_NAME[IOP_COUNT][MAX_NAME_LEN];
extern const size_t IOP_NUM_OPERANDS[IOP_COUNT];

typedef struct IOp {
    IOpType type;
    size_t operands[IOP_MAX_OPERANDS]; // operand addresses
} IOp;

/*
 * address numbering starts with parameter list, continuing into stack vars
 */
typedef struct IFuncEntry {
    const Word *name;
    IOp *ops;
    IType *params;
    size_t len, cap;
    size_t num_params;
} IFuncEntry;

typedef struct IFuncHandle { unsigned id; } IFunc;

typedef struct IRContext {
    Bump pool;

    Vec consts; // Vec<IConst *>
    Vec funcs; // Vec<IFuncEntry *>
} IRContext;

// takes in contexts and expr, returns a function index
typedef size_t (*IRHook)(Fungus *fun, IRContext *ctx, IFunc *func, Expr *expr);

void ir_init(void);

IRContext IRContext_new(void);
void IRContext_del(IRContext *);

IFunc ir_add_func(IRContext *, Word name, IType *params, size_t num_params);
size_t ir_add_op(IRContext *, IFunc, IOpType type, size_t *opers,
                 size_t num_opers);
size_t ir_add_const(IRContext *, IConst *); // copies constant to ctx

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast);
void ir_dump(IRContext *ctx);

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx);
#endif

#endif
