#include <string.h>

#include "types.h"
#include "data.h"

typedef struct TypeMap {
    TypeEntry *entries;
    size_t size, cap;
} TypeMap;

typedef struct TypeTable {
    Bump pool;
    TypeMap map; // map of name => ty
    Vec entries; // list of filled typemap entries indexed by typeid
    unsigned counter;
} TypeTable;

static TypeTable type_tab, meta_tab;

#define ATOM_MAP_INIT_CAP 16

static TypeTable TypeTable_new(void) {
    TypeTable tab = {
        .pool = Bump_new(),
        .map = (TypeMap){
            .entries = calloc(ATOM_MAP_INIT_CAP, sizeof(*tab.map.entries)),
            .cap = ATOM_MAP_INIT_CAP
        },
        .entries = Vec_new()
    };

    return tab;
}

static void TypeTable_del(TypeTable *tab) {
    free(tab->map.entries);
    Vec_del(&tab->entries);
    Bump_del(&tab->pool);
}

static TypeEntry *TypeMap_put_lower(TypeMap *map, TypeEntry *data) {
    // put in first available space
    size_t index = data->name->hash % map->cap;

    while (map->entries[index].name)
        index = (index + 1) % map->cap;

    map->entries[index] = *data;

    return &map->entries[index];
}

static void TypeTable_embiggen(TypeTable *tab) {
    TypeMap *map = &tab->map;

    // take old data
    TypeEntry *old = map->entries;
    size_t old_cap = map->cap;

    // create new map space
    map->cap *= 2;
    map->size = 0;
    map->entries = calloc(map->cap, sizeof(*map->entries));

    // reinsert data
    for (size_t i = 0; i < old_cap; ++i) {
        if (old[i].name) {
            TypeEntry *entry = TypeMap_put_lower(map, &old[i]);

            tab->entries.data[entry->id] = entry;
        }
    }

    free(old);
}

static TypeEntry *TypeMap_get(TypeMap *map, Word *name) {
    // find index of bkt (or fail)
    size_t index = name->hash % map->cap;

    while (map->entries[index].name
           && !Word_eq(map->entries[index].name, name)) {
        index = (index + 1) % map->cap;
    }

    // return
    if (map->entries[index].name)
        return &map->entries[index];

    return NULL;
}

// returns null if already defined
static TypeEntry *TypeTable_define(TypeTable *tab, Word *name) {
    TypeMap *map = &tab->map;

    // make sure this type doesn't exist yet
    if (TypeMap_get(&tab->map, name))
        return NULL;

    // check for embiggening
    if (map->size++ >= map->cap / 2)
        TypeTable_embiggen(tab);

    // store in table and return
    Word *copy = Word_copy_of(name, &tab->pool);
    TypeEntry entry = {
        .name = copy,
        .id = tab->counter++
    };

    TypeEntry *perm_entry = TypeMap_put_lower(map, &entry);

    Vec_push(&tab->entries, perm_entry);

    return perm_entry;
}

static void register_builtin_types(void) {
#define X(A) #A,
    char names[TY_COUNT][64] = { BUILTIN_TYPES };
    char metanames[MTY_COUNT][64] = { BUILTIN_METATYPES };
#undef X

    for (BuiltinType i = 0; i < TY_COUNT; ++i) {
        for (char *trav = names[i]; *trav; ++trav)
            *trav += 'a' - 'A';

        Word word = Word_new(names[i], strlen(names[i]));

        type_define(&word);
    }

    for (BuiltinMetaType i = 0; i < MTY_COUNT; ++i) {
        for (char *trav = metanames[i]; *trav; ++trav)
            *trav += 'a' - 'A';

        Word word = Word_new(metanames[i], strlen(metanames[i]));

        metatype_define(&word);
    }
}

void types_init(void) {
    type_tab = TypeTable_new();
    meta_tab = TypeTable_new();

    register_builtin_types();
}

void types_quit(void) {
    TypeTable_del(&meta_tab);
    TypeTable_del(&type_tab);
}

static void dump_table(TypeTable *tab, const char *msg) {
    printf("%*s", 8, "");
    term_format(TERM_YELLOW);
    printf("%s", msg);
    term_format(TERM_RESET);
    printf("\n");

    for (size_t i = 0; i < tab->entries.len; ++i) {
        TypeEntry *entry = (TypeEntry *)tab->entries.data[i];

        term_format(TERM_CYAN);
        printf(" %6x", entry->id);
        term_format(TERM_RESET);
        printf(" %.*s\n", (int)entry->name->len, entry->name->str);
    }
}

void types_dump(void) {
    dump_table(&type_tab, "types");
    dump_table(&meta_tab, "metatypes");
}

// api functionality ===========================================================

Type type_define(Word *name) {
    TypeEntry *entry;

    if ((entry = TypeTable_define(&type_tab, name)))
        return TYPE(entry->id);

    fungus_panic("type \"%.*s\" was defined twice.", (int)name->len, name->str);
}

TypeEntry *type_get(Type ty) {
    return type_tab.entries.data[ty.id];
}

TypeEntry *type_get_by_name(Word *name) {
    return TypeMap_get(&type_tab.map, name);
}

MetaType metatype_define(Word *name) {
    TypeEntry *entry;

    if ((entry = TypeTable_define(&meta_tab, name)))
        return METATYPE(entry->id);

    fungus_panic("metatype \"%.*s\" was defined twice.", (int)name->len, name->str);
}

TypeEntry *metatype_get(MetaType ty) {
    return meta_tab.entries.data[ty.id];
}

TypeEntry *metatype_get_by_name(Word *name) {
    return TypeMap_get(&meta_tab.map, name);
}
