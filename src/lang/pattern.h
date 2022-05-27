#ifndef PATTERN_H
#define PATTERN_H

#include "../data.h"
#include "../sema/types.h"
#include "../sema.h"
#include "../file.h"

typedef struct AstExpr AstExpr;
typedef struct Lang Lang;

typedef enum MatchType {
    MATCH_EXPR,
    MATCH_LEXEME
} MatchType;

typedef struct MatchAtom {
    MatchType type;

    union {
        // expr
        struct {
            const TypeExpr *rule_expr, *type_expr;

            // flags (used for tree generation, have no direct behavior)
            bool repeating;
            bool optional;
        };

        // lexeme
        const Word *lxm;
    };
} MatchAtom;

typedef struct WhereClause {
    const Word *name;
    const TypeExpr *type_expr;

    // indices of templated params
    size_t *constrains;
    size_t num_constrains;
    bool hits_return; // whether `returns` is constrained
} WhereClause;

typedef struct Pattern {
    // TODO should probably store names for bindings?

    MatchAtom *matches;
    size_t len;
    const TypeExpr *returns;

    WhereClause *wheres;
    size_t wheres_len;
} Pattern;

void pattern_lang_init(void);
void pattern_lang_quit(void);

// for internal use only:
extern Lang pattern_lang;

File pattern_file(const char *str);
AstExpr *precompile_pattern(Bump *, Names *names, const File *file);
Pattern compile_pattern(Bump *, Names *names, const File *file,
                        const AstExpr *ast);

bool MatchAtom_matches_rule(const File *, const MatchAtom *, const AstExpr *);
bool MatchAtom_matches_type(const MatchAtom *, const AstExpr *);
bool MatchAtom_equals(const MatchAtom *, const MatchAtom *);

void MatchAtom_print(const MatchAtom *);
void Pattern_print(const Pattern *);

#endif
