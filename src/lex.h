#ifndef RAW_H
#define RAW_H

#include <stddef.h>

#include "expr.h"
#include "fungus.h"

typedef struct RawExprBuffer {
    Expr *exprs;
    size_t len, cap;
} RawExprBuf;

Lexer Lexer_new(void);
void Lexer_del(Lexer *);

RawExprBuf tokenize(Fungus *, const char *str, size_t len);

void RawExprBuf_del(RawExprBuf *);

#endif
