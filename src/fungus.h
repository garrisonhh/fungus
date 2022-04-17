#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

#include <stddef.h>
#include <stdint.h>

typedef uint32_t hsize_t;

// table of (name, fun name, num supers, super list)
#define BASE_TYPES\
    X(any,       "Any",       0, {0}) /* any runtime value */\
    X(any_expr,  "AnyExpr",   0, {0}) /* any AST expr */\
    /* primitives */\
    X(primitive, "Primitive", 1, { fun_any })\
    X(bool,      "bool",      1, { fun_primitive })\
    X(string,    "string",    1, { fun_primitive })\
    X(int,       "int",       1, { fun_primitive })\
    X(float,     "float",     1, { fun_primitive })\
    /* rules */\
    X(rule,      "Rule",      1, { fun_any_expr })\
    X(scope,     "Scope",     1, { fun_rule })\
    X(lexeme,    "Lexeme",    1, { fun_any_expr })\
    X(literal,   "Literal",   1, { fun_any_expr })\
    X(ident,     "Ident",     1, { fun_any_expr })\

// define fun_X global vars that are defined in fungus_define_base
#define X(NAME, ...) extern Type fun_##NAME;
BASE_TYPES
#undef X

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(void);
void fungus_lang_quit(void);

#endif
