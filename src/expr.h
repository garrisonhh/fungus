#ifndef EXPR_H
#define EXPR_H

#include "types.h"

typedef struct Expr {
    Type ty; // value typing
    MetaType mty; // code typing

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

void Expr_dump(Expr *expr);

#endif
