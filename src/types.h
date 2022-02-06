#ifndef TYPES_H
#define TYPES_H

#include "words.h"

/*
 * fungus types are split between runtime (normal type) and comptime (metatype)
 */

#define BUILTIN_TYPES\
    X(NONE)\
    X(STRING)\
    X(INT)\
    X(FLOAT)\
    X(BOOL)\

#define BUILTIN_METATYPES\
    X(NONE)\
    X(LITERAL)\

#define X(A) TY_##A,
typedef enum BuiltinTypes { BUILTIN_TYPES TY_COUNT } BuiltinType;
#undef X
#define X(A) MTY_##A,
typedef enum BuiltinMetaTypes { BUILTIN_METATYPES MTY_COUNT } BuiltinMetaType;
#undef X

// these are wrapped in structs to enforce type checking
typedef struct TypeHandle { unsigned id; } Type;
typedef struct MetaTypeHandle { unsigned id; } MetaType;

#define TYPE(ID) (Type){ ID }
#define METATYPE(ID) (MetaType){ ID }

// used to store type data
typedef struct TypeEntry {
    unsigned id;
    Word *name; // entry is empty if !name
} TypeEntry;

void types_init(void);
void types_quit(void);

void types_dump(void);

Type type_define(Word *name);
TypeEntry *type_get(Type ty);
TypeEntry *type_get_by_name(Word *name);

MetaType metatype_define(Word *name);
TypeEntry *metatype_get(MetaType mty);
TypeEntry *metatype_get_by_name(Word *name);

#endif
