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

    /*
     * base types
     * noruntime - for lexemes and other metatypes with no runtime value
     */
    Type t_noruntime;
    Type t_string, t_bool, t_number, t_int, t_float;
    Type t_metatype, t_literal, t_lexeme;
} Fungus;

Fungus Fungus_new(void);
void Fungus_del(Fungus *);

Type Fungus_define_symbol(Fungus *, Word symbol);
Type Fungus_define_keyword(Fungus *, Word keyword);

void Fungus_dump(Fungus *);

#endif
