#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

#include <stddef.h>
#include <stdint.h>

typedef uint32_t hsize_t;

// TODO actually impl abstract types like Any
#define BASE_TYPES\
    X(any,     "Any")\
    /* primitives */\
    X(bool,    "bool")\
    X(string,  "string")\
    X(int,     "int")\
    X(float,   "float")\
    /* rules */\
    X(scope,   "Scope")\
    X(lexeme,  "Lexeme")\
    X(literal, "Literal")\
    X(ident,   "Ident")\

// define fun_X global vars that are defined in fungus_define_base
#define X(NAME, STR) extern Type fun_##NAME;
BASE_TYPES
#undef X

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(void);
void fungus_lang_quit(void);

#endif
