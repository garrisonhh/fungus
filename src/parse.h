#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"
#include "lang/ast_expr.h"

typedef struct AstCtx {
    Bump *pool;
    const File *file;
    const Lang *lang;
} AstCtx;

// parses scope from raw tokens
AstExpr *parse(AstCtx *, const TokBuf *);
// parses a processed scope
AstExpr *parse_scope(AstCtx *, AstExpr *);

#endif
