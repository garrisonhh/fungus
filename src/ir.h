#ifndef IR_H
#define IR_H

#include "fungus.h"

/*
 * ir uses a generally generic representation, types of any given instruction
 * must be inferred by semantic analysis + codegen via the parameters of an IOp
 */

/*
 * TODO representing algebraic types through IType:
 *
 * require accounting for two things, parameterizing and sum vs. product
 * - parameterizing is `naming` typed data
 * - algebraic types create structured data
 *
 * examples
 * - unparameterized
 *   - sum -> raw union
 *   - prod -> tuple, ptr, maybe array?
 * - parameterized
 *   - sum -> enum, tagged union
 *   - prod -> struct
 */
// table of (type, c type)
#define ITYPE_TABLE\
    /* special */\
    X(MUST_INFER,   "$MUST_INFER$")\
    X(NIL,          "void")\
    /* primitives */\
    X(BOOL,         "bool")\
    X(I64,          "int64_t")\
    X(F64,          "double")\

#define X(A, B) ITYPE_##A,
typedef enum IType { ITYPE_TABLE ITYPE_COUNT } IType;
#undef X

extern char ITYPE_NAME[ITYPE_COUNT][MAX_NAME_LEN];

typedef struct IFuncHandle { unsigned id; } IFunc;

// table of (iinst, type, c fmt)
#define IINST_TABLE\
    /* special */\
    X(NOP,      IIT_NOP,        NULL)\
    X(RET,      IIT_RET,        NULL)\
    X(CONST,    IIT_CONST,      NULL)\
    X(CALL,     IIT_CALL,       NULL)\
    /* math */\
    X(ADD,      IIT_BINARY,     "v%zu + v%zu")\
    X(SUB,      IIT_BINARY,     "v%zu - v%zu")\
    X(MUL,      IIT_BINARY,     "v%zu * v%zu")\
    X(DIV,      IIT_BINARY,     "v%zu / v%zu")\
    X(MOD,      IIT_BINARY,     "v%zu % v%zu")\
    /* bitwise */\
    X(BAND,     IIT_BINARY,     "v%zu & v%zu")\
    X(BOR,      IIT_BINARY,     "v%zu | v%zu")\
    X(BXOR,     IIT_BINARY,     "v%zu ^ v%zu")\
    X(BNOT,     IIT_UNARY,      "~v%zu")\
    X(SHL,      IIT_BINARY,     "v%zu << v%zu")\
    X(SHR,      IIT_BINARY,     "v%zu >> v%zu")\
    /* relational */\
    X(EQ,       IIT_BINARY,     "v%zu == v%zu")\
    X(NE,       IIT_BINARY,     "v%zu != v%zu")\
    X(GT,       IIT_BINARY,     "v%zu > v%zu")\
    X(LT,       IIT_BINARY,     "v%zu < v%zu")\
    X(GE,       IIT_BINARY,     "v%zu >= v%zu")\
    X(LE,       IIT_BINARY,     "v%zu <= v%zu")\
    /* logical */\
    X(LAND,     IIT_BINARY,     "v%zu && v%zu")\
    X(LOR,      IIT_BINARY,     "v%zu || v%zu")\
    X(LNOT,     IIT_UNARY,      "!v%zu")\

#define IINST_TYPE_TABLE\
    X(NOP)\
    X(RET)\
    X(CONST)\
    X(UNARY)\
    X(BINARY)\
    X(CALL)\

#define X(A, B, C) II_##A,
typedef enum IInstruction { IINST_TABLE II_COUNT } IInst;
#undef X
#define X(A) IIT_##A,
typedef enum IInstructionType { IINST_TYPE_TABLE IIT_COUNT } IInstType;
#undef X

extern char IINST_NAME[II_COUNT][MAX_NAME_LEN];
extern IInstType IINST_TYPE[II_COUNT];
extern char IINST_TYPE_NAME[IIT_COUNT][MAX_NAME_LEN];

typedef struct IOperation {
    IInst inst;
    IType ty; // if inst is not `const`, this will be inferred

    union {
        // constants
        fun_bool bool_;
        fun_int int_;
        fun_float float_;

        // operations
        struct { size_t a, b; };
        struct {
            IFunc func;
            size_t *params;
            size_t num_params;
        } call;
    };
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

    IType ret_ty;
} IFuncEntry;

typedef struct IRContext {
    Bump pool;

    Vec funcs; // Vec<IFuncEntry *>
} IRContext;

// takes in contexts and expr, returns a function index
typedef size_t (*IRHook)(Fungus *fun, IRContext *ctx, IFunc *func, Expr *expr);

void ir_init(void);

IRContext IRContext_new(void);
void IRContext_del(IRContext *);

IFuncEntry *ir_get_func(IRContext *, IFunc func);
IFunc ir_add_func(IRContext *, IType ret_ty, Word name, IType *params,
                  size_t num_params);
size_t ir_add_op(IRContext *, IFunc func, IOp op); // copies op
void ir_fprint_const(FILE *, IOp *op);

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast);
void ir_dump(IRContext *ctx);

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx);
#endif

#endif
