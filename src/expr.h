#ifndef EXPR_H
#define EXPR_H

#include "types.h"
#include "precedence.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    Type ty, cty;

    union {
        // literals
        Word string_;
        long int_;
        double float_;
        bool bool_;

        // rules
        struct {
            struct Expr **exprs;
            size_t len;

            // TODO get these out of expr somehow? or do a data- : Typeoriented memory
            // thingy?
            Prec prec;
            unsigned prefixed: 1;
            unsigned postfixed: 1;
        };
    };
} Expr;

void Expr_dump(Fungus *, Expr *);

static inline void Expr_dump_array(Fungus *fun, Expr **exprs, size_t len) {
    for (size_t i = 0; i < len; ++i)
        Expr_dump(fun, exprs[i]);
}

// returns addr of leftmost expr of a rule expr's children for ast manipulation
Expr **Expr_left(Fungus *, Expr *);

#endif
