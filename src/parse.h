#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"

#define EXPR_TYPES\
    X(INVALID)\
    X(SCOPE)\
    X(RULE)\
    X(LEXEME)\
    X(IDENT)\
    X(LIT_BOOL)\
    X(LIT_INT)\
    X(LIT_FLOAT)\
    X(LIT_STRING)\

#define X(A) EX_##A,
typedef enum ExprType { EXPR_TYPES EX_COUNT } ExprType;
#undef X

extern const char *EX_NAME[EX_COUNT];

typedef struct Expr {
    ExprType type;

    union {
        // for non-scopes
        struct { hsize_t tok_start, tok_len; };
        // for scopes + rules
        struct {
            struct Expr **exprs;
            size_t len;

            Rule rule;
        };
    };
} Expr;

// parses scope from raw tokens
Expr *parse(Bump *, const Lang *, const TokBuf *);
// parses an expr scope given a language
Expr *parse_scope(Bump *, const File *, const Lang *, Expr *);

void Expr_error(const File *, const Expr *, const char *fmt, ...);
void Expr_error_from(const File *, const Expr *, const char *fmt, ...);
void Expr_dump(const Expr *, const File *);

#endif
