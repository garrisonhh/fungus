#ifndef SCOPE_H
#define SCOPE_H

#include "scope/types.h"

/*
 * Scope is used for verifying programs during semantic analysis
 */

typedef struct Scope {
    const struct Scope *parent;

    TypeGraph types;
    // TODO name -> type idmap
} Scope;

Scope Scope_new(const Scope *parent);
bool Scope_del(Scope *);

#endif
