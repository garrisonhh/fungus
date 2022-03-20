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
 *   - this means fungus programs can be written in a sort of nicely restricted
 *     duck typing style, basically Zig's comptime system with guarantees that
 *     are closer to Rust's `where` clauses
 * - syntax patterns can be expressed as types which match `RunType * CompType`
 *   - type matching should be expressable in the language as well
 */

typedef struct TypeHandle { unsigned id; } Type;

typedef struct TypeGraph {
    Bump pool;
    Vec entries;

    IdMap by_name;
} TypeGraph;

typedef struct TypeDef {
    Word name;
    Type *is;
    size_t is_len;
} TypeDef;

extern const Type INVALID_TYPE; // TODO this is hacky, get rid of it

TypeGraph TypeGraph_new(void);
void TypeGraph_del(TypeGraph *);

Type Type_define(TypeGraph *, TypeDef *def);
bool Type_get(TypeGraph *, Word *name, Type *o_type);
const Word *Type_name(TypeGraph *, Type ty);
bool Type_is(TypeGraph *, Type ty, Type super);

void TypeGraph_dump(TypeGraph *);

#endif
