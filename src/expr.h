#ifndef EXPR_H
#define EXPR_H

#include "types.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    Type ty, cty;

    union {
        // literal
        Word string_;
        long int_;
        double float_;
        bool bool_;

        // block + rules
        struct {
            struct Expr **exprs;
            size_t len;
        };
    };
} Expr;

void Expr_dump(Fungus *, Expr *expr);

static inline void Expr_dump_array(Fungus *fun, Expr **exprs, size_t len) {
    for (size_t i = 0; i < len; ++i)
        Expr_dump(fun, exprs[i]);
}

#endif
