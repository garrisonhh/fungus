#ifndef EXPR_H
#define EXPR_H

#include "types.h"
#include "rules.h"
#include "literal_types.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    Type ty;
    bool atomic; // rule types are non-atomic

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
