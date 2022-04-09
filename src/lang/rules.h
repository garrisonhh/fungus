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

typedef struct RuleNode {
    MatchAtom *pred;
    Vec nexts;

    // rule
    Rule rule;
    unsigned has_rule: 1;
} RuleNode;

typedef struct RuleBacktrack {
    MatchAtom *pred;
    Vec backs; // points to root nodes in RuleTree
} RuleBacktrack;

/*
 * TODO for pattern compiling, Rule types will need to know about each other
 * before the patterns for rules are actually compiled. this means I will
 * have to change the rule definition system into two phases, one where
 * patterns are stored and types are generated and then a second where
 * patterns are compiled using those types and then the tree is created
 */
typedef struct RuleTree {
    Bump pool;
    Vec entries; // entries[0] represents Scope, never contains an actual entry
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

// ruletree's second phase
void RuleTree_crystallize(RuleTree *);

Rule Rule_define(RuleTree *, RuleDef *def);
Rule Rule_by_name(const RuleTree *, const Word *name);
Type Rule_typeof(const RuleTree *, Rule rule);
const RuleEntry *Rule_get(const RuleTree *, Rule rule);

void RuleTree_dump(const RuleTree *);

#endif
