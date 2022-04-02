#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"

#define EXPR_TYPES\
    X(INVALID)\
    X(LEXEME)\
    X(SCOPE)\
    X(IDENT)\
    X(LIT_BOOL)\
    X(LIT_INT)\
    X(LIT_FLOAT)\
    X(LIT_STRING)\

#define X(A) EX_##A,
typedef enum ExprType { EXPR_TYPES EX_COUNT } ExprType;
#undef X

typedef struct Scope {
    struct Expr **exprs;
    size_t len;
} Scope;

typedef struct Expr {
    ExprType type;
    hsize_t tok_start, tok_len;
    Scope scope; // only used for scopes TODO don't waste memory?
} Expr;

Expr *parse(Bump *, const Lang *, const TokBuf *);

#endif
