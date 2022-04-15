#ifndef NAME_TABLE_H
#define NAME_TABLE_H

#include "../data.h"
#include "types.h"

/*
 * a symbol table suitable for typing the AST + mapping variable references
 * within scopes
 *
 * names exist ephemerally (when a scope is left, they are removed from the
 * table)
 *
 * TODO SOA-able eventually
 */

#define NAMED_TYPES\
    X(INVALID)\
    \
    X(VARIABLE)\
    X(TYPE)\
    X(LANG)\
    X(PREC)\

#define X(NAME) NAMED_##NAME,
typedef enum NamedType { NAMED_TYPES NAMED_COUNT } NamedType;
#undef X

typedef struct NameEntry {
    NamedType type;
    Word name;
} NameEntry;

typedef struct NameTable {
    NameEntry *entries;
    size_t len, cap;
} NameTable;

NameTable NameTable_new(void);
void NameTable_del(NameTable *);

#endif
