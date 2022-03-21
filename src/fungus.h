#ifndef FUNGUS_H
#define FUNGUS_H

#include "expr.h"
#include "lex.h"
#include "types.h"
#include "precedence.h"
#include "rules.h"

typedef struct Fungus {
    Lexer lexer;
    TypeGraph types;
    PrecGraph precedences;
    RuleTree rules;

    Bump temp; // temporary pool for repl iterations

    // base types
    Type t_notype;
    Type t_runtype, t_number, t_string, t_bool, t_int, t_float;
    Type t_comptype, t_lexeme, t_literal;

    // base precedences
    Prec p_lowest, p_highest;
} Fungus;

Fungus Fungus_new(void);
void Fungus_del(Fungus *);

void *Fungus_tmp_alloc(Fungus *, size_t n_bytes);
void Fungus_tmp_clear(Fungus *);
// if children > 0, allocates for them on Expr->exprs
Expr *Fungus_tmp_expr(Fungus *, size_t children);

bool Fungus_define_symbol(Fungus *, Word symbol, Type *o_type);
bool Fungus_define_keyword(Fungus *, Word keyword, Type *o_type);

void Fungus_print_types(Fungus *, Type ty, Type cty);
size_t Fungus_sprint_types(Fungus *, char *str, Type ty, Type cty);

void Fungus_dump(Fungus *);

#endif
