#ifndef LANG_H
#define LANG_H

#include "lang/rules.h"
#include "lang/precedence.h"

/*
 * Lang stores language info, and is used during AST parsing
 */

typedef struct Lang {
    RuleTree rules;
    PrecGraph precs;
    IdMap keywords, symbols;
} Lang;

Lang Lang_new(void);
void Lang_del(Lang *);

#endif
