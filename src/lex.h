#ifndef RAW_H
#define RAW_H

#include <stddef.h>

#include "expr.h"

/*
 * TODO the lexer should just return a token stream, where tokens are just a
 * comptype and an index into the file string. all other work can be done later.
 *
 * TODO the lexer REALLY needs a rework, this is a hacky piece of crap tbh
 */

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
