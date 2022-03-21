#ifndef TYPES_H
#define TYPES_H

#include "data.h"

/*
 * TypeGraph is a generic type system representation, usable for both runtime
 * and comptime fungus types
 *
 * TODO algebraic, mutable types (probably fully replacing the type graph):
 * - instead of having type graph, types are stored in expression form. when
 *   a type is declared to 'extend' another type, it can be added as an 'or'
 *   clause to the original type.
 *   - this means fungus programs can be written in a sort of nicely comptime-
 *     restricted duck typing style, basically Zig's comptime system with
 *     guarantees that are closer to Haskell or OCaml
 * - syntax patterns can then be expressed as types
 *   - comptime type matching should be expressable in the language as well
 */

typedef struct TypeHandle { unsigned id; } Type;

typedef enum TypeExprType {
    // basic unit like `u64`
    TET_ATOM,

    // for enum tags + tagged enums TODO
    // TET_TAG,
    // TET_TAGGED,

    // algebraic typing
    TET_SUM,
    TET_PRODUCT,
} TypeExprType;

typedef struct TypeExpr {
    TypeExprType type;
    union {
        // atomic
        Type atom;

        /*
        // tag/tagged
        struct {
            const Word *tag;
            struct TypeExpr *child;
        };
        */

        // algebraic
        struct {
            struct TypeExpr **exprs;
            size_t len;
        };
    };
} TypeExpr;

typedef enum TypeType {
    TY_CONCRETE, // a new type
    TY_ABSTRACT, // a type grouping (like an interface)
    TY_ALIAS // an alias for a type expression
} TypeType;

// used to define a new type. all data is copied
typedef struct TypeDef {
    Word name;

    TypeType type;
    TypeExpr *expr; // aliased types alias

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
        // concrete types store no data
        IdSet *type_set; // abstract
        TypeExpr *expr; // aliased
    };
} TypeEntry;

typedef struct TypeGraph {
    Bump pool;
    Vec entries;

    IdMap by_name;
} TypeGraph;

TypeGraph TypeGraph_new(void);
void TypeGraph_del(TypeGraph *);

Type Type_define(TypeGraph *, TypeDef *def);
bool Type_by_name(TypeGraph *, Word *name, Type *o_type);
TypeEntry *Type_get(TypeGraph *, Type ty);
const Word *Type_name(TypeGraph *, Type ty);

TypeExpr *TypeExpr_deepcopy(Bump *pool, TypeExpr *expr);
bool TypeExpr_deepequals(TypeExpr *expr, TypeExpr *other);

bool Type_is(TypeGraph *, Type ty, Type of);
bool TypeExpr_is(TypeGraph *, TypeExpr *expr, Type of);
bool Type_matches(TypeGraph *, Type ty, TypeExpr *pat);
bool TypeExpr_matches(TypeGraph *, TypeExpr *expr, TypeExpr *pat);

// TypeExpr convenience funcs
TypeExpr *TypeExpr_atom(Bump *pool, Type ty);
TypeExpr *TypeExpr_sum(Bump *pool, size_t n, ...);
TypeExpr *TypeExpr_product(Bump *pool, size_t n, ...);

void TypeExpr_print(TypeGraph *, TypeExpr *expr);
void Type_print(TypeGraph *, Type ty);
void Type_print_verbose(TypeGraph *, Type ty);
void TypeGraph_dump(TypeGraph *);

#endif
