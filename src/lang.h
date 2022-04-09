#ifndef LANG_H
#define LANG_H

#include "lang/rules.h"
#include "lang/precedence.h"

/*
 * Lang stores language info, and is used during AST parsing
 */

// TODO could unify allocators into one Bump?
typedef struct Lang {
    Word name;
    RuleTree rules;
    PrecGraph precs;
    HashSet words, syms;
} Lang;

Lang Lang_new(Word name);
void Lang_del(Lang *);

Rule Lang_legislate(Lang *, RuleDef *);
Prec Lang_make_prec(Lang *, PrecDef *);

void Lang_dump(const Lang *);

#endif
