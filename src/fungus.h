#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"

#include <stddef.h>
#include <stdint.h>

typedef uint32_t hsize_t;

typedef _Bool           fun_bool;
typedef long long int   fun_int;
typedef long double     fun_float;

// table of enum, name, abstractness, ... (is)
#define BASE_TYPES\
    TYPE(NONE,      "NONE",      1, 0, {0})\
    \
    TYPE(PRIMITIVE, "Primitive", 1, 0, {0})\
    TYPE(NUMBER,    "Number",    1, 1, {{TY_PRIMITIVE}})\
    \
    TYPE(BOOL,      "bool",      0, 1, {{TY_PRIMITIVE}})\
    TYPE(STRING,    "string",    0, 1, {{TY_PRIMITIVE}})\
    TYPE(INT,       "int",       0, 1, {{TY_NUMBER}})\
    TYPE(FLOAT,     "float",     0, 1, {{TY_NUMBER}})\

#define TYPE(A, ...) TY_##A,
typedef enum BaseType { BASE_TYPES TY_COUNT } BaseType;
#undef TYPE

extern Lang fungus_lang;
extern TypeGraph fungus_types;

void fungus_lang_init(void);
void fungus_lang_quit(void);

void fungus_types_init(void);
void fungus_types_quit(void);

#endif
