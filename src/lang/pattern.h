#ifndef PATTERN_H
#define PATTERN_H

#include "../data.h"
#include "../types.h"

typedef struct AstExpr AstExpr;
typedef struct Lang Lang;

/*
 * TODO:
 * - match should be able to differentiate lexemes, scopes, rules, and other
 *   (this is a `parse` problem)
 * - type checking should then be able to check scopes, rules, and other for
 *   runtime type information
 *   (this is a `sema` problem)
 */

typedef enum MatchType {
    MATCH_EXPR,
    MATCH_LEXEME
} MatchType;

typedef struct MatchAtom {
    MatchType type;

    union {
        // expr
        struct {
            const TypeExpr *rule_expr; // RuleTree Type typeexpr (maybe NULL)
            const TypeExpr *type_expr; // Type typeexpr (maybe NULL)
        };

        // lexeme
        const Word *lxm;
    };
} MatchAtom;

typedef struct Pattern {
    MatchAtom *matches;
    size_t len;
    const TypeExpr *returns;

    // TODO template (relative type) stuff
} Pattern;

void pattern_lang_init(void);
void pattern_lang_quit(void);

bool MatchAtom_equals(const MatchAtom *, const MatchAtom *);

AstExpr *precompile_pattern(Bump *, const char *str);
Pattern compile_pattern(Bump *, const Lang *lang, AstExpr *ast);

void MatchAtom_print(const MatchAtom *, const TypeGraph *rule_types,
                     const TypeGraph *types);
void Pattern_print(const Pattern *, const TypeGraph *rule_types,
                   const TypeGraph *types);

#endif
