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
    Type t_runtype, t_string, t_bool, t_number, t_int, t_float;
    Type t_comptype, t_literal, t_lexeme, t_parens;

    // base precedences
    Prec p_lowest, p_highest;
} Fungus;

Fungus Fungus_new(void);
void Fungus_del(Fungus *);

void *Fungus_tmp_alloc(Fungus *, size_t n_bytes);
void Fungus_tmp_clear(Fungus *);

Type Fungus_define_symbol(Fungus *, Word symbol);
Type Fungus_define_keyword(Fungus *, Word keyword);

void Fungus_print_types(Fungus *, Type ty, Type mty);
size_t Fungus_sprint_types(Fungus *, char *str, Type ty, Type mty);

void Fungus_dump(Fungus *);

#endif
