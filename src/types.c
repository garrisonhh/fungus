#include "types.h"

#define MAX_TYPES 256

typedef struct TypeEntry {
    const Word *name;

    // list of types this type also is
    // TODO smarter data structure (zB a hash set)
    Type is[MAX_TYPES];
    size_t is_len;
} TypeEntry;

TypeGraph TypeGraph_new(void) {
    return (TypeGraph){
        .pool = Bump_new(),
        .entries = Vec_new()
    };
}

void TypeGraph_del(TypeGraph *tg) {
    Vec_del(&tg->entries);
    Bump_del(&tg->pool);
}

static void *TG_alloc(TypeGraph *tg, size_t n_bytes) {
    return Bump_alloc(&tg->pool, n_bytes);
}

static TypeEntry *TG_get(TypeGraph *tg, Type handle) {
    if (handle.id > tg->entries.len)
        fungus_panic("attempted to get an invalid type.");

    return tg->entries.data[handle.id];
}

Type Type_define(TypeGraph *tg, TypeDef *def) {
    TypeEntry *entry = TG_alloc(tg, sizeof(*entry));
    Type handle = (Type){ tg->entries.len };

    *entry = (TypeEntry){
        .name = Word_copy_of(&def->name, &tg->pool),
        .is_len = def->is_len
    };

    for (size_t i = 0; i < def->is_len; ++i)
        entry->is[i] = def->is[i];

    Vec_push(&tg->entries, entry);

    return handle;
}

const Word *Type_name(TypeGraph *tg, Type ty) {
    return TG_get(tg, ty)->name;
}

bool Type_is(TypeGraph *tg, Type ty, Type other) {
    TypeEntry *entry = TG_get(tg, ty);

    // check direct parents
    for (size_t i = 0; i < entry->is_len; ++i)
        if (entry->is[i].id == other.id)
            return true;

    // check indirect parents
    for (size_t i = 0; i < entry->is_len; ++i)
        if (Type_is(tg, entry->is[i], other))
            return true;

    return false;
}

void TypeGraph_dump(TypeGraph *tg) {
    for (size_t i = 0; i < tg->entries.len; ++i) {
        TypeEntry *entry = tg->entries.data[i];

        printf("  %.*s\n", (int)entry->name->len, entry->name->str);

        for (size_t j = 0; j < entry->is_len; ++j) {
            const Word *parent_name = TG_get(tg, entry->is[j])->name;

            printf("    %.*s\n", (int)parent_name->len, parent_name->str);
        }

        puts("");
    }
}
