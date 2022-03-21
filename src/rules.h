#ifndef RULES_H
#define RULES_H

#include "types.h"
#include "precedence.h"

#ifndef MAX_RULE_LEN
#define MAX_RULE_LEN 64
#endif

typedef struct Fungus Fungus;

/*
 * patterns are defined something like this:
 *
 * (Pattern){
 *     .pat = { 0, 1, 0 }, .len = 3,
 *     .returns = 0,
 *     .where = {
 *         { number },
 *         { plus_lexeme }
 *     }
 * }
 *
 * this is exactly like some rust-like syntax:
 *
 * fn add(lhs: T, symbol: S, rhs: T): T
 *    where T: Number,
 *          S: +;
 *
 * where typeexprs are deepcopied!
 */
typedef struct Pattern {
    size_t *pat;
    size_t len;
    size_t returns;
    TypeExpr **where;
    size_t where_len;
} Pattern;

typedef enum Associativity { ASSOC_LEFT, ASSOC_RIGHT } Associativity;

/*
 * fill this out in order to define a rule
 * when defined, every rule will also generate its own comptype using the name
 * provided
 */
typedef struct RuleDef {
    Word name;
    Pattern pat;
    Prec prec;
    Associativity assoc;
} RuleDef;

typedef struct RuleHandle { unsigned id; } Rule;

// used to store rule data within ruletree entries
typedef struct RuleEntry {
    Rule rule;
    Type ty;

    Pattern pat;
    Prec prec;
    Associativity assoc;

    // flags
    unsigned prefixed: 1; // if rule has a lexeme at start
    unsigned postfixed: 1; // if rule has a lexeme at end
} RuleEntry;

// nodes could use hashmaps instead of vecs to store nexts, but I'm not sure
// this would actually result in a performance improvement in most cases
typedef struct RuleNode {
    Vec nexts;
    TypeExpr *te;

    Rule rule;
    unsigned terminates: 1; // whether `rule` is valid
    unsigned repeats: 1; // whether `nexts` contains a self-reference
} RuleNode;

typedef struct RuleTree {
    Bump pool;
    Vec entries;
    RuleNode *root; // dummy rulenode, contains no valid type info
} RuleTree;

RuleTree RuleTree_new(void);
void RuleTree_del(RuleTree *);

Rule Rule_define(Fungus *, RuleDef *def);
RuleEntry *Rule_get(RuleTree *, Rule rule);

void RuleTree_dump(Fungus *, RuleTree *);

#endif
