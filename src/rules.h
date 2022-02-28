#ifndef RULES_H
#define RULES_H

#include "types.h"
#include "precedence.h"
#include "expr.h"

enum PatternNodeModifiers {
    PAT_REPEAT = 0x1,
    // PAT_OPTIONAL = 0x2, TODO unimplementable with current Rule_define impl,
    // need a recursive impl I think
};

typedef struct PatNode {
    Type ty;
    unsigned modifiers; // bitfield using modifiers
} PatNode;

typedef Expr *(*RuleHook)(Expr *);

// fill this out in order to define a rule
typedef struct RuleDef {
    PatNode *pattern;
    size_t len;
    Prec prec;
    Type ty, mty;
    RuleHook interpret;
} RuleDef;

// used to store rule data at the end of an atom
typedef struct Rule {
    Prec prec;
    Type ty, mty; // the types of the rule's value, and of the rule itself
    RuleHook interpret; // NULL for subrules (fragments of a rule)
} Rule;

// nodes could use hashmaps instead of vecs to store nexts, but I'm not sure
// this would actually result in a performance improvement
typedef struct RuleNode {
    struct RuleNode *parent; // null if this node is a root node
    Rule *rule; // non-null if node terminates

    Type ty;
    Vec nexts;
} RuleNode;

typedef struct RuleTree {
    Bump pool;
    RuleNode *root; // dummy rulenode, contains no valid type info
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

Rule *Rule_define(Fungus *, RuleDef *def);

void RuleTree_dump(Fungus *, RuleTree *);

#endif
