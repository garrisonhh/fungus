#ifndef RULES_H
#define RULES_H

#include <stddef.h>

#include "precedence.h"
#include "pattern.h"
#include "../data.h"

typedef struct AstExpr AstExpr;

/*
 * RuleTree is for storing rule data throughout AST parsing
 */

typedef struct RuleHandle { unsigned id; } Rule;

// used to store rule data within ruletree entries
typedef struct RuleEntry {
    const Word *name;
    union {
        AstExpr *pre_pat; // compiled during crystallization
        Pattern pat;
    };
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
     * allow AstExprs + Pattern TypeExprs to have a universal type system
     *
     * TODO should probably move this to Lang
     */
    TypeGraph types;

    // 'constants'
    Rule rule_scope;
    Type ty_scope, ty_any, ty_literal, ty_lexeme, ty_ident;

#ifdef DEBUG
    unsigned crystallized: 1;
#endif
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

// used by PatternLang exclusively, skips the pattern compilation step and
// places rule in tree. crystallize is still necessary for backtracking.
Rule Rule_immediate_define(RuleTree *, Word name, Prec prec, Pattern pattern);

// first phase: queue rule definitions
Rule Rule_define(RuleTree *, Word name, Prec prec, AstExpr *pat_ast);
// second phase: compiling + applying queued definitions
void RuleTree_crystallize(RuleTree *);

Type Rule_typeof(const RuleTree *, Rule rule);
Rule Rule_by_name(const RuleTree *, const Word *name);
const RuleEntry *Rule_get(const RuleTree *, Rule rule);

void RuleTree_dump(const RuleTree *);

#endif
