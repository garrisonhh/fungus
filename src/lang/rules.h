#ifndef RULES_H
#define RULES_H

#include <stddef.h>

#include "precedence.h"
#include "pattern.h"
#include "../data.h"
#include "../types.h"

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
    Type type;
} RuleEntry;

#define RULENODE_NEXT_LEN 64

typedef struct RuleNode {
    MatchAtom *pred;
    Vec nexts;

    // rule
    Rule rule;
    unsigned has_rule: 1;
} RuleNode;

typedef struct RuleBacktrack {
    MatchAtom *pred;

    Vec back_atoms;
    Vec back_nodes;
} RuleBacktrack;

typedef struct RuleTree {
    Bump pool;
    Vec entries; // entries[0] represents Scope, nevre contains an actual entry
    IdMap by_name;

    Vec roots; // Vec<RuleNode *>
    Vec backtracks; // Vec<RuleBackTrack *>

    /*
     * this is the internal type system for Rules, used for Pattern matching
     * RuleTree's TypeGraph also includes some special types that aren't
     * associated with a rule, zB literals, lexemes, and idents. this is to
     * allow RExprs + Pattern TypeExprs to have a universal type system
     *
     * TODO should probably move this to Lang
     */
    TypeGraph types;

    // 'constants'
    Rule rule_scope;
    Type ty_scope, ty_any, ty_literal, ty_lexeme, ty_ident;
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

Rule Rule_define(RuleTree *, RuleDef *def);
Rule Rule_by_name(const RuleTree *, const Word *name);
Type Rule_typeof(const RuleTree *, Rule rule);
const RuleEntry *Rule_get(const RuleTree *, Rule rule);

void RuleTree_dump(const RuleTree *);

#endif
