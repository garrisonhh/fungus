#ifndef SYNTAX_H
#define SYNTAX_H

#include "types.h"
#include "lex.h"

/*
 * TODO precedence as a directed graph. atm precedence is simply first-come-
 * first-serve with highest precedences declared first.
 */

typedef unsigned PrecId;

// TODO might want to rename this to something like PatternAtom or smth?
typedef struct SyntaxPattern {
    // if ty or mty is NONE, pattern matches any ty or mty
    Type ty;
    MetaType mty;
} Pattern;

typedef struct SyntaxRule {
    Pattern *pattern;
    size_t len;
    PrecId prec;

    Type ty;
    MetaType mty;

    // TODO some understanding of what this rule represents?
} Rule;

// context for one repl iteration
typedef struct IterContext {
    Bump pool;
} IterCtx;

PrecId prec_unique_id(Word *name);
int prec_cmp(PrecId a, PrecId b); // > 0 means a > b, < 0 means a < b

void syntax_init(void);
void syntax_quit(void);

void syntax_dump(void);

MetaType syntax_def_block(Word *name, MetaType start, MetaType stop);
void syntax_def_rule(Rule *rule); // deepcopies rule

IterCtx IterCtx_new(void);
void IterCtx_del(IterCtx *ctx);

void Expr_dump(Expr *expr);

Expr *syntax_parse(IterCtx *ctx, Expr *exprs, size_t len);

#endif
