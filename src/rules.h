#ifndef RULES_H
#define RULES_H

#include "data.h"
#include "types.h"
#include "precedence.h"
#include "lex.h"

typedef enum RuleAtomModifiers {
    RAM_REPEAT = 0x1,
    RAM_OPTIONAL = 0x2,
} RuleAtomModifier;

typedef struct RuleAtom {
    // if ty or mty is NONE, pattern matches any ty or mty
    Type ty;
    MetaType mty;

    unsigned modifiers; // bitfield
} RuleAtom;

typedef Expr *(*RuleHook)(Expr *);

// used to store rule data at the end of an atom
typedef struct Rule {
    PrecId prec;
    Type ty; // type returned by rule
    MetaType mty; // type of rule itself
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
    PrecId prec;
    Type ty;
    MetaType mty;
    RuleHook interpret;
} RuleDef;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

void RuleTree_define_rule(RuleTree *, RuleDef *def);

void RuleTree_dump(RuleTree *);

#endif
