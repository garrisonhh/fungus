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
        struct {
            const File *pre_file;
            AstExpr *pre_pat; // compiled during crystallization
        };
        Pattern pat;
    };
    Prec prec;
    Type type;
} RuleEntry;

typedef struct RuleNode {
    // TODO RuleNode only really uses the rule expr of the MatchAtom, shouldn't
    // I just store that instead of the whole thing?
    MatchAtom *pred;
    Vec nexts;

    // rule
    Rule rule;
    bool has_rule;
} RuleNode;

typedef struct RuleTree {
    Bump pool;
    Vec entries; // entries[0] represents Scope, never contains an actual entry
    IdMap by_name;
    Vec roots; // Vec<RuleNode *>

    // 'constants'; available for every Lang
    Rule rule_scope;

#ifdef DEBUG
    bool crystallized;
#endif
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

Type Rule_define_type(Names *, Word name);

// used by PatternLang exclusively in order to skip pattern compiling
// (it can't compile itself, lol)
Rule Rule_immediate_define(RuleTree *, Type type, Prec prec, Pattern pattern);

// first phase: queue rule definitions
Rule Rule_define(RuleTree *, const File *, Type type, Prec prec,
                 AstExpr *pat_ast);
// second phase: compiling + applying queued definitions
void RuleTree_crystallize(RuleTree *, Names *);

Type Rule_typeof(const RuleTree *, Rule rule);
Rule Rule_by_name(const RuleTree *, const Word *name);
const RuleEntry *Rule_get(const RuleTree *, Rule rule);

void RuleTree_dump(const RuleTree *);

#endif
