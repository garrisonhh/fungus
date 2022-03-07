#ifndef EXPR_H
#define EXPR_H

#include "literal_types.h"
#include "types.h"
#include "precedence.h"
#include "rules.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    Type ty, cty;

    union {
        // literals
        Word string_;
        fun_int int_;
        fun_float float_;
        fun_bool bool_;

        // rules
        struct {
            struct Expr **exprs;
            size_t len;

            Rule rule;
        };
    };
} Expr;

void Expr_dump(Fungus *, Expr *);
void Expr_dump_array(Fungus *, Expr **exprs, size_t len);

#endif
