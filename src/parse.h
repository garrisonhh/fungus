#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"
#include "lang/ast_expr.h"

// parses scope from raw tokens
AstExpr *parse(Bump *, const Lang *, const TokBuf *);
// parses an expr scope given a language, returns NULL on failure
AstExpr *parse_scope(Bump *, const File *, const Lang *, AstExpr *);

#endif
