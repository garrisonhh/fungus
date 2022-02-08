#ifndef RAW_H
#define RAW_H

#include <stddef.h>

#include "expr.h"

// raw expr generation =========================================================

typedef struct RawExprBuffer {
    Expr *exprs;
    size_t len, cap;
} RawExprBuf;

void lex_init(void);
void lex_quit(void);

// both can set global_error
MetaType lex_def_symbol(Word *symbol);
MetaType lex_def_keyword(Word *keyword);

RawExprBuf tokenize(const char *str, size_t len);

void RawExprBuf_del(RawExprBuf *);

#endif
