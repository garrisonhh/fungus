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
    Precs precs;

    HashSet words, syms;
} Lang;

Lang Lang_new(Word name);
void Lang_del(Lang *);

Rule Lang_legislate(Lang *, const File *, Type type, Prec prec,
                    AstExpr *pat_ast);
Rule Lang_immediate_legislate(Lang *, Type type, Prec prec, Pattern pat);
Prec Lang_make_prec(Lang *, Word name, Associativity assoc);
void Lang_crystallize(Lang *, Names *);

// for zig
HashSet *Lang_syms(Lang *);
HashSet *Lang_words(Lang *);

void Lang_dump(const Lang *);

#endif
