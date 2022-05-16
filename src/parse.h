#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

/*
 * TODO I think that sema will need to handle the interpretation and re-feeding
 * of fungus languages back through the lexer and parser; would make a much
 * cleaner solution than doing it in the parse method
 */

typedef struct AstExpr AstExpr;
typedef struct Lang Lang;

typedef struct AstCtx {
    Bump *pool;
    const File *file;
    const Lang *lang;
} AstCtx;

// parses scope into an AST from raw tokens
AstExpr *parse(AstCtx *, const TokBuf *);

#endif
