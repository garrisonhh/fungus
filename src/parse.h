#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"

#define EXPR_TYPES\
    X(INVALID)\
    X(SCOPE)\
    X(RULE)\
    \
    X(LEXEME)\
    X(LIT_LEXEME)\
    X(IDENT)\
    X(LIT_BOOL)\
    X(LIT_INT)\
    X(LIT_FLOAT)\
    X(LIT_STRING)\

#define X(A) REX_##A,
typedef enum RawExprType { EXPR_TYPES REX_COUNT } RExprType;
#undef X

extern const char *REX_NAME[REX_COUNT];

typedef struct RawExpr {
    RExprType type;

    union {
        // for non-scopes
        struct { hsize_t tok_start, tok_len; };
        // for scopes + rules
        // TODO should I store scope Lang somehow?
        struct {
            Rule rule;
            struct RExpr **exprs;
            size_t len;
        };
    };
} RExpr;

// parses scope from raw tokens
RExpr *parse(Bump *, const Lang *, const TokBuf *);
// parses an expr scope given a language, returns NULL on failure
RExpr *parse_scope(Bump *, const File *, const Lang *, RExpr *);

void RExpr_error(const File *, const RExpr *, const char *fmt, ...);
void RExpr_error_from(const File *, const RExpr *, const char *fmt, ...);
void RExpr_dump(const RExpr *, const Lang *, const File *);

#endif
