#ifndef AST_EXPR_H
#define AST_EXPR_H

#include "rules.h"
#include "../file.h"
#include "../sema/types.h"

typedef struct Lang Lang;

// TODO get rid of this ideally
#define MAX_AST_DEPTH 2048

typedef struct AstExpr {
    // `type` is AST type; `evaltype` is type this evaluates to
    Type type, evaltype;

    union {
        // for atoms
        struct {
            hsize_t tok_start, tok_len;
        };

        // for rules (rules include all composite AST nodes)
        struct {
            Rule rule;
            struct AstExpr **exprs;
            size_t len;
        };
    };
} AstExpr;

bool AstExpr_is_atom(const AstExpr *);
Word AstExpr_as_word(const File *, const AstExpr *expr);

void AstExpr_display(const File *, const AstExpr *);
void AstExpr_error(const File *, const AstExpr *, const char *fmt, ...);
void AstExpr_error_from(const File *, const AstExpr *, const char *fmt, ...);
void AstExpr_dump(const AstExpr *, const Lang *, const File *);

// for zig interop
typedef struct AstExprTok {
    hsize_t start, len;
} AstExprTok;

typedef struct AstExprRule {
    Rule rule;
    AstExpr **exprs;
    size_t len;
} AstExprRule;

AstExprTok AstExpr_tok(const AstExpr *);
AstExprRule AstExpr_rule(const AstExpr *);

#endif
