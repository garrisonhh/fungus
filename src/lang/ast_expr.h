#ifndef AST_EXPR_H
#define AST_EXPR_H

#include "rules.h"
#include "../fun_types.h"
#include "../types.h"

typedef struct File File;
typedef struct Lang Lang;

// TODO get rid of this ideally
#define MAX_AST_DEPTH 2048

#define ATOM_TYPES\
    X(INVALID)\
    \
    X(IDENT)\
    X(LEXEME)\
    X(BOOL)\
    X(INT)\
    X(FLOAT)\
    X(STRING)\

#define X(A) ATOM_##A,
typedef enum AstAtomType { ATOM_TYPES ATOM_COUNT } AstAtomType;
#undef X

extern const char *ATOM_NAME[ATOM_COUNT];

typedef struct AstExpr {
    Type type; // on RuleTree TypeGraph
    bool is_atom; // TODO can I get rid of this

    union {
        // for atoms
        struct {
            AstAtomType atom_type;
            hsize_t tok_start, tok_len;
        };

        // for rules
        struct {
            Rule rule;
            struct AstExpr **exprs;
            size_t len;
        };
    };
} AstExpr;

void AstExpr_error(const File *, const AstExpr *, const char *fmt, ...);
void AstExpr_error_from(const File *, const AstExpr *, const char *fmt, ...);
void AstExpr_dump(const AstExpr *, const Lang *, const File *);

#endif
