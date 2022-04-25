#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

// table of (name, fun name, num supers, super list)
#define BASE_TYPES\
    /* special */\
    X(unknown,    "Unknown",     0, {0}) /* placeholder for parsing */\
    X(any,        "Any",         0, {0}) /* truly any valid type */\
    X(any_val,    "AnyValue",    1, { fun_any }) /* any runtime value */\
    X(any_expr,   "AnyExpr",     1, { fun_any }) /* any AST expr */\
    X(rule,       "Rule",        1, { fun_any_expr })\
    X(nil,        "nil",         1, { fun_any_val })\
    /* metatypes + parsing */\
    X(scope,      "Scope",       1, { fun_rule })\
    X(lexeme,     "Lexeme",      1, { fun_any_expr })\
    X(literal,    "Literal",     1, { fun_any_expr })\
    X(ident,      "Ident",       1, { fun_any_expr })\
    X(type,       "Type",        1, { fun_any_expr }) /* fungus type */\
    /* pattern types */\
    X(match,      "Match",       1, { fun_rule }) /* type!evaltype */\
    X(match_expr, "MatchExpr",   1, { fun_rule })\
    X(type_or,    "TypeOr",      1, { fun_rule })\
    X(type_bang,  "TypeBang",    1, { fun_rule })\
    X(returns,    "Returns",     1, { fun_rule })\
    X(pattern,    "Pattern",     1, { fun_rule })\
    X(wh_clause,  "WhereClause", 1, { fun_rule })\
    X(where,      "Where",       1, { fun_rule })\
    /* primitives */\
    X(primitive,  "Primitive",   1, { fun_any_val })\
    X(bool,       "bool",        1, { fun_primitive })\
    X(string,     "string",      1, { fun_primitive })\
    X(number,     "Number",      1, { fun_primitive })\
    X(int,        "int",         1, { fun_number })\
    X(float,      "float",       1, { fun_number })\

// define fun_X global vars that are defined in fungus_define_base
#define X(NAME, ...) extern Type fun_##NAME;
BASE_TYPES
#undef X

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(Names *);
void fungus_lang_quit(void);

#endif
