#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "../data.h"

typedef struct Names Names;

/*
 * TypeGraph is how fungus stores types within scopes.
 *
 * TODO deprecate in favor of NameTable
 */

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

        // TODO hashing TypeExprs? and TypeSet for sum types
    };
} TypeExpr;

void types_init(void);
void types_quit(void);

void types_dump(void);

Type Type_define(Names *, Word name, Type *supers, size_t num_supers);
const Word *Type_name(Type);

TypeExpr *TypeExpr_deepcopy(Bump *pool, const TypeExpr *expr);
bool TypeExpr_deepequals(const TypeExpr *expr, const TypeExpr *other);

bool Type_is(Type ty, Type of);
bool TypeExpr_is(const TypeExpr *expr, Type of);
bool Type_matches(Type ty, const TypeExpr *pat);
bool TypeExpr_matches(const TypeExpr *expr, const TypeExpr *pat);

// exact recursive equality
bool TypeExpr_equals(const TypeExpr *, const TypeExpr *);

// TypeExpr convenience funcs
TypeExpr *TypeExpr_atom(Bump *pool, Type ty);
TypeExpr *TypeExpr_sum(Bump *pool, size_t n, ...);
TypeExpr *TypeExpr_product(Bump *pool, size_t n, ...);

void TypeExpr_print(const TypeExpr *expr);
void Type_print(Type ty);

#endif
