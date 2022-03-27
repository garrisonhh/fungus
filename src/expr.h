#ifndef EXPR_H
#define EXPR_H

#include "types.h"
#include "rules.h"
#include "fun_types.h"

typedef struct Fungus Fungus;

typedef struct Expr {
    struct Expr *parent;

    Type ty;
    bool atomic; // rule types are non-atomic; literals + lexemes + idents are

    union {
        Word ident;

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

Expr **Expr_lhs(Expr *);
Expr **Expr_rhs(Expr *);

void Expr_dump(Fungus *, Expr *);
void Expr_dump_array(Fungus *, Expr **exprs, size_t len);
void Expr_dump_depth(Fungus *, Expr *, int limit);
void Expr_dump_array_depth(Fungus *, Expr **exprs, size_t len, int limit);

#endif
