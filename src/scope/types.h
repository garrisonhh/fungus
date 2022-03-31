#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "../data.h"

/*
 * TypeGraph is how fungus stores types within scopes.
 */

// table of enum, name, abstractness, ... (is)
#define BASE_TYPES\
    TYPE(NONE,      "NONE",      1, 0, {0})\
    \
    TYPE(RUNTYPE,   "RunType",   1, 0, {0})\
    TYPE(PRIMITIVE, "Primitive", 1, 1, {{TY_RUNTYPE}})\
    TYPE(NUMBER,    "Number",    1, 1, {{TY_PRIMITIVE}})\
    \
    TYPE(BOOL,      "bool",      0, 1, {{TY_PRIMITIVE}})\
    TYPE(STRING,    "string",    0, 1, {{TY_PRIMITIVE}})\
    TYPE(INT,       "int",       0, 1, {{TY_NUMBER}})\
    TYPE(FLOAT,     "float",     0, 1, {{TY_NUMBER}})\
    \
    TYPE(COMPTYPE,  "CompType",  1, 0, {0})\
    TYPE(IDENT,     "Ident",     0, 1, {{TY_COMPTYPE}})\

#define TYPE(A, ...) TY_##A,
typedef enum BaseType { BASE_TYPES TY_COUNT } BaseType;
#undef TYPE

typedef struct TypeHandle { unsigned id; } Type;

typedef enum TypeExprType {
    // basic unit like `u64`
    TET_ATOM,

    // algebraic typing
    TET_SUM,
    TET_PRODUCT,
} TypeExprType;

typedef struct TypeExpr {
    TypeExprType type;
    union {
        // atomic
        Type atom;

        // algebraic
        struct {
            struct TypeExpr **exprs;
            size_t len;
        };
    };
} TypeExpr;

typedef enum TypeType {
    TTY_CONCRETE, // a new type
    TTY_ABSTRACT, // a type grouping (like an interface)
} TypeType;

// used to define a new type. all data is copied
typedef struct TypeDef {
    Word name;
    TypeType type;

    struct { // all types can subtype abstract types
        Type *is;
        size_t is_len;
    };
} TypeDef;

// used to store data about various types
typedef struct TypeEntry {
    const Word *name;

    TypeType type;
    union {
        IdSet *type_set; // abstract
    };
} TypeEntry;

typedef struct TypeGraph {
    Bump pool;
    Vec entries;
    IdMap by_name;

    uint16_t id;
} TypeGraph;

TypeGraph TypeGraph_new(void);
void TypeGraph_del(TypeGraph *);

// fungus base types need to be defined for top level scope
void TypeGraph_define_base(TypeGraph *);

Type Type_define(TypeGraph *, TypeDef *def);
bool Type_by_name(const TypeGraph *, Word *name, Type *o_type);
// returns if this handle comes from this typegraph
bool Type_is_in(const TypeGraph *, Type ty);
TypeEntry *Type_get(const TypeGraph *, Type ty);
const Word *Type_name(const TypeGraph *, Type ty);

TypeExpr *TypeExpr_deepcopy(Bump *pool, TypeExpr *expr);
bool TypeExpr_deepequals(TypeExpr *expr, TypeExpr *other);

bool Type_is(const TypeGraph *, Type ty, Type of);
bool TypeExpr_is(const TypeGraph *, TypeExpr *expr, Type of);
bool Type_matches(const TypeGraph *, Type ty, TypeExpr *pat);
bool TypeExpr_matches(const TypeGraph *, TypeExpr *expr, TypeExpr *pat);

// TypeExpr convenience funcs
TypeExpr *TypeExpr_atom(Bump *pool, Type ty);
TypeExpr *TypeExpr_sum(Bump *pool, size_t n, ...);
TypeExpr *TypeExpr_product(Bump *pool, size_t n, ...);

void TypeExpr_print(const TypeGraph *, TypeExpr *expr);
void Type_print(const TypeGraph *, Type ty);
void Type_print_verbose(const TypeGraph *, Type ty);
void TypeGraph_dump(const TypeGraph *);

#endif
