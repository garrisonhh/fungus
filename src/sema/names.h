#ifndef NAMES_H
#define NAMES_H

#include "../data.h"
#include "types.h"

/*
 * this is a symbol table implementation for use in typing + typechecking the
 * AST in sema.
 *
 * there are two ways sym
 */

#define NAMED_TYPES\
    X(INVALID)\
    \
    X(VARIABLE)\
    X(TYPE)\
    // TODO X(PREC)\
    // TODO X(LANG)

#define X(NAME) NAMED_##NAME,
typedef enum NamedType { NAMED_TYPES NAMED_COUNT } NamedType;
#undef X

typedef struct NameEntry {
    const Word *name;

    NamedType type;
    union {
        // types
        const TypeExpr *type_expr;

        // vars
        Type var_type;
    };
} NameEntry;

/*
 * trivially optimizable into a hashmap eventually
 *
 * scopes[0] isn't really used; at level 0 entries go into a global symbol table
 */
typedef struct Names {
    Bump pool;

    NameEntry *entries;
    size_t len, cap;

    size_t *scopes;
    size_t level, scope_cap;
} Names;

void names_init(void);
void names_quit(void);

Names Names_new(void);
void Names_del(Names *);

void Names_push_scope(Names *);
void Names_drop_scope(Names *);

void Names_define_type(Names *, const Word *name, const TypeExpr *type_expr);
void Names_define_var(Names *, const Word *name, Type type);

const NameEntry *name_lookup(const Names *, const Word *name);

#endif
