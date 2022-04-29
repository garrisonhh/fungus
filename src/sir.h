#ifndef SIR_H
#define SIR_H

#include <assert.h>

#include "lang/ast_expr.h"

/*
 * 'Structured Intermediate Representation', a second AST
 */

#define SIR_TYPE_TYPES\
    X(NIL)\
    X(IDENT)\
    \
    X(BOOL)\
    X(I64)\
    X(F64)\

#define X(NAME) ST_##NAME,
typedef enum SirTypeType { SIR_TYPE_TYPES ST_COUNT } SirTypeType;
#undef X

typedef struct SirType {
    SirTypeType type;

    // eventually will want more complicated types here
} SirType;

#define SIR_EXPR_TYPES\
    X(LITERAL)\
    X(SCOPE)\
    X(CONST_DECL)\
    X(LET_DECL)\
    \
    X(ADD)\
    X(SUB)\
    X(MUL)\
    X(DIV)\
    X(MOD)

#define X(NAME) SE_##NAME,
typedef enum SirExprType { SIR_EXPR_TYPES SE_COUNT } SirExprType;
#undef X

static_assert(sizeof(double) == 8, "SirExpr f64 type must be 64 bits.");

typedef struct SirExpr {
    SirExprType type;
    SirType evaltype;

    union {
        // literals
        int64_t i64;
        double f64;
        bool bool_;

        // operands
        struct {
            const struct SirExpr *a, *b, *c;
        };

        // scopes
        struct {
            const struct SirExpr **exprs;
            size_t len;
        };
    };
} SirExpr;

const SirExpr *generate_sir(Bump *, const File *, const AstExpr *ast);

void SirExpr_dump(const SirExpr *);

#endif
