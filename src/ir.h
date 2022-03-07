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
 * -
 *
 * examples
 * - unparameterized
 *   - sum -> raw union
 *   - prod -> tuple, ptr
 * - parameterized
 *   - sum -> enum, tagged union
 *   - prod -> struct
 */
#define ITYPE_TABLE\
    X(BOOL)\
    X(I64)\
    X(F64)\

#define X(A) ITYPE_##A,
typedef enum IType { ITYPE_TABLE ITYPE_COUNT } IType;
#undef X

extern char ITYPE_NAME[ITYPE_COUNT][MAX_NAME_LEN];

typedef struct IConst {
    IType type;
    union {
        fun_bool bool_;
        fun_int int_;
        fun_float float_;
    };
} IConst;

typedef struct IFuncHandle { unsigned id; } IFunc;

// table of (iinst, type)
#define IINST_TABLE\
    /* special */\
    X(NOP,      IIT_NIL)\
    X(RET,      IIT_UNARY)\
    X(CONST,    IIT_CONST)\
    X(CALL,     IIT_CALL)\
    /* math */\
    X(ADD,      IIT_BINARY)\
    X(SUB,      IIT_BINARY)\
    X(MUL,      IIT_BINARY)\
    X(DIV,      IIT_BINARY)\
    X(MOD,      IIT_BINARY)\

#define IINST_TYPE_TABLE\
    X(NIL)\
    X(CONST)\
    X(UNARY)\
    X(BINARY)\
    X(CALL)\

#define X(A, B) II_##A,
typedef enum IInstruction { IINST_TABLE II_COUNT } IInst;
#undef X
#define X(A) IIT_##A,
typedef enum IInstructionType { IINST_TYPE_TABLE IIT_COUNT } IInstType;
#undef X

extern char IINST_NAME[II_COUNT][MAX_NAME_LEN];
extern IInstType IINST_TYPE[II_COUNT];
extern char IINST_TYPE_NAME[IIT_COUNT][MAX_NAME_LEN];

typedef struct IOpUnary { size_t a; } IOpUnary;

typedef struct IOperation {
    IInst inst;
    union {
        IConst iconst;
        struct { size_t a; } unary;
        struct { size_t a, b; } binary;
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

IFunc ir_add_func(IRContext *, Word name, IType *params, size_t num_params);
size_t ir_add_op(IRContext *, IFunc func, IOp op); // copies op

void ir_gen(Fungus *fun, IRContext *ctx, Expr *ast);
void ir_dump(IRContext *ctx);

#ifdef DEBUG
void ir_test(Fungus *fun, IRContext *ctx);
#endif

#endif
