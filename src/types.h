#ifndef TYPES_H
#define TYPES_H

#include "words.h"

/*
 * TypeGraph is a generic type system representation, usable for both runtime
 * and comptime fungus types
 *
 * current features:
 * - directed acyclic type graph (Int and Float are also Number)
 */

typedef struct TypeHandle { unsigned id; } Type;

typedef struct TypeGraph {
    Bump pool;
    Vec entries;

    // TODO name => id hashmap
} TypeGraph;

typedef struct TypeDef {
    Word name;
    Type *is;
    size_t is_len;
} TypeDef;

TypeGraph TypeGraph_new(void);
void TypeGraph_del(TypeGraph *);

Type Type_define(TypeGraph *, TypeDef *def);
const Word *Type_name(TypeGraph *, Type ty);
bool Type_is(TypeGraph *, Type ty, Type other);

void TypeGraph_dump(TypeGraph *);

#endif
