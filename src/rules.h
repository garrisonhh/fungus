#ifndef RULES_H
#define RULES_H

#include "types.h"
#include "precedence.h"
#include "expr.h"

typedef enum RuleAtomModifiers {
    RAM_REPEAT = 0x1,
    RAM_OPTIONAL = 0x2,
} RuleAtomModifier;

typedef struct RuleAtom {
    Type mty;
    unsigned modifiers; // bitfield using RuleAtomModifiers
} RuleAtom;

typedef Expr *(*RuleHook)(Expr *);

// used to store rule data at the end of an atom
typedef struct Rule {
    Prec prec;
    Type ty, mty; // the types of the rule's value, and of the rule itself
    RuleHook interpret; // NULL for subrules (fragments of a rule)
} Rule;

// nodes could use hashmaps instead of vecs to store nexts, but I'm not sure
// this would actually result in a performance improvement
typedef struct RuleTreeNode {
    Vec nexts;
    RuleAtom atom;
    Rule *rule;
} RTNode;

typedef struct RuleTree {
    Bump pool;
    Vec roots;
} RuleTree;

// fill this out in order to define a rule
typedef struct RuleDef {
    RuleAtom *pattern;
    size_t len;
    Prec prec;
    Type ty, mty;
    RuleHook interpret;
} RuleDef;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

void Rule_define(RuleTree *, RuleDef *def);

void RuleTree_dump(Fungus *, RuleTree *);

#endif
