#ifndef RULES_H
#define RULES_H

#include <stddef.h>

#include "precedence.h"
#include "pattern.h"
#include "../data.h"

/*
 * RuleTree is for storing rule data throughout AST parsing
 */

/*
 * fill this out in order to define a rule
 * when defined, every rule will also generate its own comptype using the name
 * provided
 */
typedef struct RuleDef {
    Word name;
    Prec prec;
    Pattern pat;
} RuleDef;

typedef struct RuleHandle { unsigned id; } Rule;

// used to store rule data within ruletree entries
typedef struct RuleEntry {
    const Word *name;
    Pattern pat;
    Prec prec;
} RuleEntry;

#define RULENODE_NEXT_LEN 64

typedef struct RuleNode {
    Rule rule; // can check validity with Rule_is_valid

    struct RuleNode *next_expr; // if node can be followed by expr
    struct RuleNode *next_lxm[RULENODE_NEXT_LEN];
    const Word *next_lxm_type[RULENODE_NEXT_LEN];
    size_t num_next_lxms;
} RuleNode;

typedef struct RuleTree {
    Bump pool;
    Vec entries;
    RuleNode *root; // dummy rulenode, contains no valid type info
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

Rule Rule_define(RuleTree *, RuleDef *def);
bool Rule_is_valid(Rule rule);
const RuleEntry *Rule_get(const RuleTree *, Rule rule);

void RuleTree_dump(const RuleTree *);

#endif
