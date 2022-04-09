#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "lang.h"

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
typedef enum RawAtomType { ATOM_TYPES ATOM_COUNT } RAtomType;
#undef X

extern const char *ATOM_NAME[ATOM_COUNT];

typedef struct RuleExpr {
    Type type; // on RuleTree TypeGraph
    bool is_atom; // TODO can I get rid of this

    union {
        // for atoms
        struct {
            RAtomType atom_type;
            hsize_t tok_start, tok_len;
        };

        // for rules
        struct {
            Rule rule;
            struct RuleExpr **exprs;
            size_t len;
        };
    };
} RExpr;

// parses scope from raw tokens
RExpr *parse(Bump *, const Lang *, const TokBuf *);
// parses an expr scope given a language, returns NULL on failure
RExpr *parse_scope(Bump *, const File *, const Lang *, RExpr *);

void RExpr_error(const File *, const RExpr *, const char *fmt, ...);
void RExpr_error_from(const File *, const RExpr *, const char *fmt, ...);
void RExpr_dump(const RExpr *, const Lang *, const File *);

#endif
