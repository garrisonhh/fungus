#ifndef EXPR_H
#define EXPR_H

#include "types.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    Type ty, mty;

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

void Expr_dump(Fungus *fun, Expr *expr);

#endif
