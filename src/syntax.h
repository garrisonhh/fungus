#ifndef SYNTAX_H
#define SYNTAX_H

#include "fungus.h"

/*
 * TODO precedence as a directed graph. atm precedence is simply first-come-
 * first-serve with highest precedences declared first.
 */

// context for one repl iteration
typedef struct IterContext {
    Bump pool;
} IterCtx;

IterCtx IterCtx_new(void);
void IterCtx_del(IterCtx *ctx);

Expr *parse(Fungus *, IterCtx *ctx, Expr *exprs, size_t len);

#endif
