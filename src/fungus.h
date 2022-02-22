#ifndef FUNGUS_H
#define FUNGUS_H

#include "expr.h"
#include "types.h"
#include "precedence.h"
#include "rules.h"

// TODO lookups are dumb stupid just-get-it-working design, have so much easy
// optimization potential
typedef struct Lexer {
    Bump pool;
    Vec symbols, keywords;
} Lexer;

typedef struct Fungus {
    Lexer lexer;
    TypeGraph types;
    PrecGraph precedences;
    RuleTree rules;

    /*
     * base types
     *
     * noruntime is for filling expr->ty on lexemes
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
