#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "../data.h"

/*
 * TypeGraph is how fungus stores types within scopes.
 *
 * TODO deprecate in favor of NameTable
 */

typedef struct TypeHandle { uint32_t id; } Type;

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

        // TODO hashing TypeExprs? and TypeSet for sum types
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
        IdSet *type_set; // for abstract types
    };
} TypeEntry;

typedef struct TypeGraph {
    Bump pool;
    Vec entries;
    IdMap by_name;

    Type ty_any;
    uint16_t id;
} TypeGraph;

TypeGraph TypeGraph_new(void);
void TypeGraph_del(TypeGraph *);

Type Type_define(TypeGraph *, TypeDef *def);
bool Type_by_name(const TypeGraph *, Word *name, Type *o_type);
// returns if this handle comes from this typegraph
bool Type_is_in(const TypeGraph *, Type ty);
TypeEntry *Type_get(const TypeGraph *, Type ty);
const Word *Type_name(const TypeGraph *, Type ty);

TypeExpr *TypeExpr_deepcopy(Bump *pool, const TypeExpr *expr);
bool TypeExpr_deepequals(const TypeExpr *expr, const TypeExpr *other);

bool Type_is(const TypeGraph *, Type ty, Type of);
bool TypeExpr_is(const TypeGraph *, const TypeExpr *expr, Type of);
bool Type_matches(const TypeGraph *, Type ty, const TypeExpr *pat);
bool TypeExpr_matches(const TypeGraph *, const TypeExpr *expr,
                      const TypeExpr *pat);

// exact recursive equality
bool TypeExpr_equals(const TypeExpr *, const TypeExpr *);

// TypeExpr convenience funcs
TypeExpr *TypeExpr_atom(Bump *pool, Type ty);
TypeExpr *TypeExpr_sum(Bump *pool, size_t n, ...);
TypeExpr *TypeExpr_product(Bump *pool, size_t n, ...);

void TypeExpr_print(const TypeGraph *, const TypeExpr *expr);
void Type_print(const TypeGraph *, Type ty);
void Type_print_verbose(const TypeGraph *, Type ty);
void TypeGraph_dump(const TypeGraph *);

#endif
