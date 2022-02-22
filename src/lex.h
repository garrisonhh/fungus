#ifndef RAW_H
#define RAW_H

#include <stddef.h>

#include "expr.h"

typedef struct Fungus Fungus; // forward decl to fix header hell

typedef struct RawExprBuffer {
    Expr *exprs;
    size_t len, cap;
} RawExprBuf;

// TODO lookups are dumb stupid just-get-it-working design, have so much easy
// optimization potential
typedef struct Lexer {
    Bump pool;
    Vec symbols, keywords;
} Lexer;

Lexer Lexer_new(void);
void Lexer_del(Lexer *);

void Lexer_dump(Lexer *);

RawExprBuf tokenize(Fungus *, const char *str, size_t len);
void RawExprBuf_del(RawExprBuf *);

void RawExprBuf_dump(Fungus *, RawExprBuf *);

#endif
