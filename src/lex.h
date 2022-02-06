#ifndef RAW_H
#define RAW_H

#include <stddef.h>

#include "words.h"
#include "types.h"

// raw expr generation =========================================================

typedef struct Expr {
    MetaType mty; // code typing
    Type ty; // value typing

    union {
        // literal
        Word _string;
        long _int;
        double _float;
        bool _bool;

        // block + rules
        struct {
            struct Expr **exprs;
            size_t len;
        };
    };
} Expr;

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
