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
            unsigned repeating: 1;
            unsigned optional: 1;
            unsigned: 0;
        };

        // lexeme
        const Word *lxm;
    };
} MatchAtom;

typedef struct WhereClause {
    const Word *name;
    const TypeExpr *type_expr;
} WhereClause;

typedef struct Pattern {
    // TODO should probably store names for bindings?

    MatchAtom *matches;
    size_t len;
    const TypeExpr *returns;

    const WhereClause *wheres;
    size_t wheres_len;
} Pattern;

void pattern_lang_init(Names *);
void pattern_lang_quit(void);

// for internal use only:
File pattern_file(const char *str);
AstExpr *precompile_pattern(Bump *, Names *names, const File *file);
Pattern compile_pattern(Bump *, const Names *names, const File *file,
                        const AstExpr *ast);

bool MatchAtom_equals(const MatchAtom *, const MatchAtom *);

void MatchAtom_print(const MatchAtom *);
void Pattern_print(const Pattern *);

#endif
