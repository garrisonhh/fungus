#ifndef SYNTAX_H
#define SYNTAX_H

#include "types.h"
#include "lex.h"
#include "rules.h"

/*
 * TODO precedence as a directed graph. atm precedence is simply first-come-
 * first-serve with highest precedences declared first.
 */

// context for one repl iteration
typedef struct IterContext {
    Bump pool;
} IterCtx;

void syntax_init(void);
void syntax_quit(void);

void syntax_dump(void);

MetaType syntax_def_block(Word *name, MetaType start, MetaType stop);
void syntax_def_rule(Rule *rule); // deepcopies rule

IterCtx IterCtx_new(void);
void IterCtx_del(IterCtx *ctx);

Expr *syntax_parse(IterCtx *ctx, Expr *exprs, size_t len);

#endif
