#ifndef RULES_H
#define RULES_H

#include "types.h"
#include "precedence.h"
#include "expr.h"

#ifndef MAX_RULE_LEN
#define MAX_RULE_LEN 64
#endif

enum PatternNodeModifiers {
    PAT_REPEAT = 0x1,
    // PAT_OPTIONAL = 0x2, TODO
};

typedef struct PatNode {
    Type ty;
    unsigned modifiers; // bitfield using modifiers
} PatNode;

typedef struct Rule Rule;

// RuleHooks take an untyped expr filled with raw pattern match data as
// children, and adds type info + whatever else it needs to do
typedef void (*RuleHook)(Fungus *fun, Rule *rule, Expr *expr);

// fill this out in order to define a rule
typedef struct RuleDef {
    Word name;
    PatNode *pattern;
    size_t len;
    Prec prec;
    Type cty;
    RuleHook hook;
} RuleDef;

// used to store rule data at the end of an atom
struct Rule {
    Word *name;
    Prec prec;
    Type cty; // available for use with some rules, must probably won't use it
    RuleHook hook;
};

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
